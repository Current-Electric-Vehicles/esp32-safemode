# Include generated list of web asset files
set(WEB_EMBED_FILES "")
set(WEB_ASSETS_CMAKE "${CMAKE_CURRENT_LIST_DIR}/web_assets.cmake")
if(EXISTS "${WEB_ASSETS_CMAKE}")
    include("${WEB_ASSETS_CMAKE}")
    message(STATUS "Web assets: ${WEB_EMBED_FILES}")
else()
    message(STATUS "No web_assets.cmake found, building without frontend")
endif()

# Generate a fallback web_assets.h if it doesn't exist (e.g., CI without frontend build)
set(WEB_ASSETS_HEADER "${CMAKE_CURRENT_LIST_DIR}/src/web_assets.h")
if(NOT EXISTS "${WEB_ASSETS_HEADER}")
    file(WRITE "${WEB_ASSETS_HEADER}"
        "#pragma once\n"
        "// Auto-generated fallback — no frontend assets embedded\n"
        "#include <cstddef>\n"
        "#include <cstdint>\n"
        "\n"
        "struct WebAsset { const char* urlPath; const char* mimeType; const uint8_t* data; size_t size; bool isGzipped; };\n"
        "static constexpr int kWebAssetCount = 0;\n"
        "inline const WebAsset kWebAssets[] = {{nullptr, nullptr, nullptr, 0, false}};\n"
    )
    message(STATUS "Generated fallback web_assets.h")
endif()

idf_component_register(
    SRCS
        "src/wifi_ap.cpp"
        "src/http_server.cpp"
        "src/dns_server.cpp"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "src"
    EMBED_FILES
        ${WEB_EMBED_FILES}
    REQUIRES
        esp_wifi
        esp_netif
        esp_event
        esp_http_server
    PRIV_REQUIRES
        ota
        esp_timer
        app_update
        factory_reset
)
