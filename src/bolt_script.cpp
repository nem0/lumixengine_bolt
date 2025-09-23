#include "bolt_script.h"
#include "core/stream.h"

namespace Lumix {


BoltScript::BoltScript(const Path& path, ResourceManager& resource_manager, IAllocator& allocator)
	: Resource(path, resource_manager, allocator)
	, m_allocator(allocator, m_path.c_str())
	, m_source_code(m_allocator)
{
}

BoltScript::~BoltScript() = default;

void BoltScript::unload() {
	m_source_code = "";
}

bool BoltScript::load(Span<const u8> mem) {
	InputMemoryStream blob(mem.begin(), mem.length());
	m_source_code = StringView((const char*)blob.skip(0), (u32)blob.remaining());
	return true;
}

} // namespace Lumix