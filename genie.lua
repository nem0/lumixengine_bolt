if plugin "bolt" then
	files { 
		"external/**.c",
		"external/**.h",
		"src/**.c",
		"src/**.cpp",
		"src/**.h",
		"genie.lua"
	}
	includedirs { "external/" }
	defines { "BUILDING_BOLT", "_CRT_SECURE_NO_WARNINGS" }
	dynamic_link_plugin { "engine" }
end