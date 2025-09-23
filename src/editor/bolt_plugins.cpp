#include "core/allocator.h"
#include "core/profiler.h"
#include "engine/engine.h"
#include "editor/action.h"
#include "editor/asset_compiler.h"
#include "editor/asset_browser.h"
#include "editor/editor_asset.h"
#include "editor/studio_app.h"
#include "editor/utils.h"
#include "editor/world_editor.h"
#include "imgui/imgui.h"
#include "../bolt_script.h"

namespace Lumix {


static inline const u32 token_colors[] = {
	IM_COL32(0xFF, 0x00, 0xFF, 0xff),
	IM_COL32(0xe1, 0xe1, 0xe1, 0xff),
	IM_COL32(0xf7, 0xc9, 0x5c, 0xff),
	IM_COL32(0xFF, 0xA9, 0x4D, 0xff),
	IM_COL32(0xFF, 0xA9, 0x4D, 0xff),
	IM_COL32(0xE5, 0x8A, 0xC9, 0xff),
	IM_COL32(0x93, 0xDD, 0xFA, 0xff),
	IM_COL32(0x67, 0x6b, 0x6f, 0xff),
	IM_COL32(0x67, 0x6b, 0x6f, 0xff)
};

enum class BoltTokenType : u8 {
	EMPTY,
	IDENTIFIER,
	NUMBER,
	STRING,
	KEYWORD,
	OPERATOR,
	COMMENT,
	COMMENT_MULTI,
	PREPROCESSOR
};

static bool isWordChar(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static bool tokenize(const char* str, u32& token_len, u8& token_type, u8 prev_token_type) {
	static const char* keywords[] = {
		"if",
		"let",
		"const",
		"fn",
		"return",
		"type",
		"if",
		"else",
		"for",
		"in",
		"to",
		"by",
		"true",
		"false",
		"null",
		"and",
		"or",
		"not",
		"import",
		"export",
		"as",
		"from",
		"is",
		"final",
		"unsealed",
		"typeof",
		"enum",
		"break",
		"continue",
		"do",
		"then",
		"match",
	};

	const char* c = str;
	if (!*c) {
		switch (prev_token_type) {
			case (u8)BoltTokenType::COMMENT_MULTI:
				token_type = prev_token_type;
				break;
			default:
				token_type = (u8)BoltTokenType::EMPTY;
				break;
		}
		token_len = 0;
		return false;
	}

	if (prev_token_type == (u8)BoltTokenType::COMMENT_MULTI) {
		token_type = (u8)BoltTokenType::COMMENT;
		while (*c) {
			if (c[0] == '*' && c[1] == '/') {
				c += 2;
				token_len = u32(c - str);
				return *c;
			}
			++c;
		}
			
		token_type = (u8)BoltTokenType::COMMENT_MULTI;
		token_len = u32(c - str);
		return *c;
	}

	if (c[0] == '#') {
		token_type = (u8)BoltTokenType::PREPROCESSOR;
		while (*c) ++c;
			
		token_len = u32(c - str);
		return false;
	}

	if (c[0] == '/' && c[1] == '*') {
		while (*c) {
			if (c[0] == '*' && c[1] == '/') {
				c += 2;
				token_type = (u8)BoltTokenType::COMMENT;
				token_len = u32(c - str);
				return *c;
			}
			++c;
		}
			
		token_type = (u8)BoltTokenType::COMMENT_MULTI;
		token_len = u32(c - str);
		return *c;
	}

	if (*c == '/' && c[1] == '/') {
		token_type = (u8)BoltTokenType::COMMENT;
		while (*c) ++c;
		token_len = u32(c - str);
		return *c;
	}

	if (*c == '"') {
		token_type = (u8)BoltTokenType::STRING;
		++c;
		while (*c && *c != '"') ++c;
		if (*c == '"') ++c;
		token_len = u32(c - str);
		return *c;
	}

	if (*c == '\'') {
		token_type = (u8)BoltTokenType::STRING;
		++c;
		while (*c && *c != '\'') ++c;
		if (*c == '\'') ++c;
		token_len = u32(c - str);
		return *c;
	}

	const char operators[] = "*/+-%.<>;=(),:[]{}&|^";
	for (char op : operators) {
		if (*c == op) {
			token_type = (u8)BoltTokenType::OPERATOR;
			token_len = 1;
			return *c;
		}
	}
		
	if (*c >= '0' && *c <= '9') {
		token_type = (u8)BoltTokenType::NUMBER;
		while (*c >= '0' && *c <= '9') ++c;
		if (*c == '.') {
			++c;
			while (*c >= '0' && *c <= '9') ++c;
		}
		token_len = u32(c - str);
		return *c;
	}

	if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || *c == '_') {
		token_type = (u8)BoltTokenType::IDENTIFIER;
		while (isWordChar(*c)) ++c;
		token_len = u32(c - str);
		StringView token_view(str, str + token_len);
		for (const char* kw : keywords) {
			if (equalStrings(kw, token_view)) {
				token_type = (u8)BoltTokenType::KEYWORD;
				break;
			}
		}
		return *c;
	}

	token_type = (u8)BoltTokenType::IDENTIFIER;
	token_len = 1;
	++c;
	return *c;
}


UniquePtr<CodeEditor> createBoltCodeEditor(StudioApp& app) {
	UniquePtr<CodeEditor> editor = createCodeEditor(app);
	editor->setTokenColors(Span(token_colors));
	editor->setTokenizer(&tokenize);
	return editor.move();
}

struct BoltEditorWindow : AssetEditorWindow {
	BoltEditorWindow(const Path& path, StudioApp& app)
		: AssetEditorWindow(app)
		, m_app(app)
		, m_path(path)
	{
		m_file_async_handle = app.getEngine().getFileSystem().getContent(path, makeDelegate<&BoltEditorWindow::onFileLoaded>(this));
	}

	~BoltEditorWindow() {
		if (m_file_async_handle.isValid()) {
			m_app.getEngine().getFileSystem().cancel(m_file_async_handle);
		}
	}

	void onFileLoaded(Span<const u8> data, bool success) {
		m_file_async_handle = FileSystem::AsyncHandle::invalid();
		if (success) {
			StringView v;
			v.begin = (const char*)data.begin();
			v.end = (const char*)data.end();
			m_code_editor = createBoltCodeEditor(m_app);
			m_code_editor->setText(v);
			m_is_code_editor_appearing = true;
		}
	}

	void save() {
		OutputMemoryStream blob(m_app.getAllocator());
		m_code_editor->serializeText(blob);
		m_app.getAssetBrowser().saveResource(m_path, blob);
		m_dirty = false;
	}
	
	void markDirty() {
		m_dirty = true;
	}

	void windowGUI() override {
		PROFILE_BLOCK("bolt editor gui");
		CommonActions& actions = m_app.getCommonActions();

		if (ImGui::BeginMenuBar()) {
			if (actions.save.iconButton(m_dirty, &m_app)) save();
			if (actions.open_externally.iconButton(true, &m_app)) m_app.getAssetBrowser().openInExternalEditor(m_path);
			if (actions.view_in_browser.iconButton(true, &m_app)) m_app.getAssetBrowser().locate(m_path);
			ImGui::EndMenuBar();
		}

		if (m_file_async_handle.isValid()) {
			ImGui::TextUnformatted("Loading...");
			return;
		}

		if (m_code_editor) {
			if (m_is_code_editor_appearing || ImGui::IsWindowAppearing()) m_code_editor->focus();
			m_is_code_editor_appearing = false;
			if (m_code_editor->gui("codeeditor", ImVec2(0, 0), m_app.getMonospaceFont(), m_app.getDefaultFont())) {
				markDirty();
			}
		}
	}
	
	const Path& getPath() override { return m_path; }
	const char* getName() const override { return "bolt script editor"; }

	StudioApp& m_app;
	FileSystem::AsyncHandle m_file_async_handle = FileSystem::AsyncHandle::invalid();
	Path m_path;
	UniquePtr<CodeEditor> m_code_editor;
	bool m_is_code_editor_appearing = false;
};

struct BoltAssetPlugin : AssetBrowser::IPlugin, AssetCompiler::IPlugin {
	explicit BoltAssetPlugin(StudioApp& app)
		: m_app(app)
	{
		app.getAssetCompiler().registerExtension("bolt", BoltScript::TYPE);
	}

	void openEditor(const Path& path) override {
		IAllocator& allocator = m_app.getAllocator();
		UniquePtr<BoltEditorWindow> win = UniquePtr<BoltEditorWindow>::create(allocator, path, m_app);
		m_app.getAssetBrowser().addWindow(win.move());
	}

	bool compile(const Path& src) override {
		return m_app.getAssetCompiler().copyCompile(src);
	}

	const char* getIcon() const override { return ICON_FA_FILE_CODE; }
	const char* getLabel() const override { return "Bolt script"; }
	ResourceType getResourceType() const override { return BoltScript::TYPE; }
	bool canCreateResource() const override { return true; }
	const char* getDefaultExtension() const override { return "bolt"; }

	void createResource(OutputMemoryStream& blob) override {}

	StudioApp& m_app;
};


struct BoltEditorPlugin : StudioApp::GUIPlugin {
	BoltEditorPlugin(StudioApp& app)
		: m_app(app)
		, m_asset_plugin(app)
	{
		const char* extensions[] = { "bolt" };
		m_app.getAssetCompiler().addPlugin(m_asset_plugin, Span(extensions));
		m_app.getAssetBrowser().addPlugin(m_asset_plugin, Span(extensions));
	}

	void onGUI() override {}
	
	const char* getName() const override { return "bolt"; }

	StudioApp& m_app;
	BoltAssetPlugin m_asset_plugin;
	float m_some_value = 0;
};


LUMIX_STUDIO_ENTRY(bolt) {
	auto* plugin = LUMIX_NEW(app.getAllocator(), BoltEditorPlugin)(app);
	app.addPlugin(*plugin);
	return nullptr;
}


}