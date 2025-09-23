#pragma once

#include "engine/resource.h"
#include "core/string.h"
#include "core/tag_allocator.h"

namespace Lumix {

struct BoltScript final : Resource {
	BoltScript(const Path& path, ResourceManager& resource_manager, IAllocator& allocator);
	virtual ~BoltScript();

	ResourceType getType() const override { return TYPE; }

	void unload() override;
	bool load(Span<const u8> mem) override;
	StringView getSourceCode() const { return m_source_code; }

	static inline const ResourceType TYPE = ResourceType("bolt_script");

private:
	TagAllocator m_allocator;
	String m_source_code;
};


} // namespace Lumix