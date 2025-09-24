#include "core/log.h"
#include "core/path.h"
#include "core/stream.h"
#include "core/tag_allocator.h"
#include "engine/engine.h"
#include "engine/file_system.h"
#include "engine/plugin.h"
#include "engine/world.h"
#include "imgui/imgui.h"
#include "bolt.h"
#include "boltstd/boltstd.h"


using namespace Lumix;


struct BoltSystem : ISystem {
	BoltSystem(Engine& engine) : m_engine(engine) {}

	const char* getName() const override { return "bolt"; }
	
	void serialize(OutputMemoryStream& serializer) const override {}
	bool deserialize(i32 version, InputMemoryStream& serializer) override {
		return version == 0;
	}

	void createModules(World& world) override;

	void initBegin() override {
		bt_Handlers handlers = bt_default_handlers();
		handlers.write = [](bt_Context* ctx, const char* msg) {
			logInfo(msg);
		};
		bt_open(&m_context, &handlers);
		boltstd_open_all(m_context);
		bt_append_module_path(m_context, "%s");
	}
	
	void shutdownStarted() override {
		bt_close(m_context);
		m_context = nullptr;
	}

	Engine& m_engine;
	bt_Context* m_context = nullptr;
};


struct BoltModule : IModule {
	BoltModule(Engine& engine, BoltSystem& system, World& world, IAllocator& allocator)
		: m_engine(engine)
		, m_system(system)
		, m_world(world)
		, m_allocator(allocator, "bolt")
	{}

	void startGame() override {
		bt_Context* ctx = m_system.m_context;
		m_main_thread = bt_make_thread(ctx);
		FileSystem& fs = m_engine.getFileSystem();
		OutputMemoryStream main_content(m_allocator);
		if (fs.getContentSync(Path("scripts/main.bolt"), main_content)) {
			main_content.write(0);
			bt_Module* module = bt_compile_module(ctx, (const char*)main_content.data(), "scripts/main");
			if (module && bt_execute(ctx, (bt_Callable*)module)) {
				m_update_func = bt_module_get_export(module, BT_VALUE_CSTRING(ctx, "update"));
			}
		}
	}

	void stopGame() override {
		bt_destroy_thread(m_system.m_context, m_main_thread);
	}

	const char* getName() const override { return "bolt"; }

	void serialize(struct OutputMemoryStream& serializer) override {
	}

	void deserialize(struct InputMemoryStream& serializer, const struct EntityMap& entity_map, i32 version) override {

	}
	ISystem& getSystem() const override { return m_system; }
	World& getWorld() override { return m_world; }
	
	void update(float time_delta) {
		if (BT_IS_NULL(m_update_func)) return;

		bt_Context* ctx = m_system.m_context;
		bt_Thread* thread = m_main_thread;
		bt_push(thread, m_update_func);
		bt_push(thread, BT_VALUE_NUMBER(time_delta));
		bt_call(thread, 1);
		bt_pop(thread);
	}

	Engine& m_engine;
	BoltSystem& m_system;
	World& m_world;
	TagAllocator m_allocator;
	bt_Thread* m_main_thread = nullptr;
	bt_Value m_update_func = BT_VALUE_NULL;
};

void BoltSystem::createModules(World& world) {
	IAllocator& allocator = m_engine.getAllocator();
	UniquePtr<BoltModule> module = UniquePtr<BoltModule>::create(allocator, m_engine, *this, world, allocator);
	world.addModule(module.move());
}

LUMIX_PLUGIN_ENTRY(bolt)
{
	return LUMIX_NEW(engine.getAllocator(), BoltSystem)(engine);
}


