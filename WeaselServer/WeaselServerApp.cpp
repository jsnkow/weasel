#include "stdafx.h"
#include "WeaselServerApp.h"
#include <filesystem>
#include <cctype>
#include <cstdlib>
#include <map>

WeaselServerApp::WeaselServerApp()
    : m_handler(std::make_unique<RimeWithWeaselHandler>(&m_ui)),
      tray_icon(m_ui) {
  // m_handler.reset(new RimeWithWeaselHandler(&m_ui));
  m_server.SetRequestHandler(m_handler.get());
  SetupMenuHandlers();
}

WeaselServerApp::~WeaselServerApp() {}

int WeaselServerApp::Run() {
  if (!m_server.Start())
    return -1;

  // win_sparkle_set_appcast_url("http://localhost:8000/weasel/update/appcast.xml");
  win_sparkle_set_registry_path("Software\\Rime\\Weasel\\Updates");
  if (GetThreadUILanguage() ==
      MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL))
    win_sparkle_set_lang("zh-TW");
  else if (GetThreadUILanguage() ==
           MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED))
    win_sparkle_set_lang("zh-CN");
  else
    win_sparkle_set_lang("en");
  win_sparkle_init();
  m_ui.Create(m_server.GetHWnd());

  m_handler->Initialize();
  m_handler->OnUpdateUI([this]() { tray_icon.Refresh(); });
  JkReloadSettingsGui();  // [jk]

  tray_icon.Create(m_server.GetHWnd());
  tray_icon.Refresh();

  int ret = m_server.Run();

  m_handler->Finalize();
  m_ui.Destroy();
  tray_icon.RemoveIcon();
  win_sparkle_cleanup();

  return ret;
}

void WeaselServerApp::SetupMenuHandlers() {
  std::filesystem::path dir = install_dir();
  m_server.AddMenuHandler(ID_WEASELTRAY_QUIT,
                          [this] { return m_server.Stop() == 0; });
  m_server.AddMenuHandler(ID_WEASELTRAY_DEPLOY,
                          std::bind(execute, dir / L"WeaselDeployer.exe",
                                    std::wstring(L"/deploy")));
  m_server.AddMenuHandler(
      ID_WEASELTRAY_SETTINGS,
      std::bind(execute, dir / L"WeaselDeployer.exe", std::wstring()));
  m_server.AddMenuHandler(
      ID_WEASELTRAY_DICT_MANAGEMENT,
      std::bind(execute, dir / L"WeaselDeployer.exe", std::wstring(L"/dict")));
  m_server.AddMenuHandler(
      ID_WEASELTRAY_SYNC,
      std::bind(execute, dir / L"WeaselDeployer.exe", std::wstring(L"/sync")));
  m_server.AddMenuHandler(ID_WEASELTRAY_WIKI,
                          std::bind(open, L"https://rime.im/docs/"));
  m_server.AddMenuHandler(ID_WEASELTRAY_HOMEPAGE,
                          std::bind(open, L"https://rime.im/"));
  m_server.AddMenuHandler(ID_WEASELTRAY_FORUM,
                          std::bind(open, L"https://rime.im/discuss/"));
  m_server.AddMenuHandler(ID_WEASELTRAY_CHECKUPDATE, check_update);
  m_server.AddMenuHandler(ID_WEASELTRAY_INSTALLDIR, std::bind(explore, dir));
  m_server.AddMenuHandler(ID_WEASELTRAY_USERCONFIG,
                          std::bind(explore, WeaselUserDataPath()));
  m_server.AddMenuHandler(ID_WEASELTRAY_LOGDIR,
                          std::bind(explore, WeaselLogPath()));
  // [jk] settings form (tray item and hotkey share this id)
  m_server.AddMenuHandler(ID_WEASELTRAY_JK_SETTINGS,
                          [this] { return JkOpenSettingsGui(); });
  // [jk] the form posts WM_COMMAND(ID_WEASELTRAY_JK_RELOAD) to the server
  // window after Apply + deploy, so a changed hotkey takes effect at once
  m_server.AddMenuHandler(ID_WEASELTRAY_JK_RELOAD, [this] {
    JkReloadSettingsGui();
    return true;
  });
}

// ---- [jk] DirectPinyin settings form --------------------------------------

namespace {
// "Ctrl+Alt+F12" / "Control+Shift+S" → modifiers + virtual key (0 = invalid)
UINT JkParseHotkey(const std::string& spec, UINT* mods) {
  *mods = 0;
  std::string key;
  size_t pos = 0;
  while (pos <= spec.size()) {
    size_t next = spec.find('+', pos);
    // a trailing "+" is the key itself (e.g. "Ctrl++" is not supported anyway)
    std::string part = spec.substr(
        pos, next == std::string::npos ? std::string::npos : next - pos);
    std::string low = part;
    for (auto& c : low)
      c = (char)tolower((unsigned char)c);
    if (low == "ctrl" || low == "control")
      *mods |= MOD_CONTROL;
    else if (low == "alt")
      *mods |= MOD_ALT;
    else if (low == "shift")
      *mods |= MOD_SHIFT;
    else if (low == "win" || low == "super")
      *mods |= MOD_WIN;
    else
      key = part;
    if (next == std::string::npos)
      break;
    pos = next + 1;
  }
  if (key.empty())
    return 0;
  if (key.size() == 1) {
    char c = (char)toupper((unsigned char)key[0]);
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
      return (UINT)c;
  }
  if ((key[0] == 'F' || key[0] == 'f') && key.size() <= 3) {
    int n = atoi(key.c_str() + 1);
    if (n >= 1 && n <= 24)
      return VK_F1 + n - 1;
  }
  static const std::map<std::string, UINT> names = {
      {"space", VK_SPACE},          {"tab", VK_TAB},
      {"return", VK_RETURN},        {"enter", VK_RETURN},
      {"escape", VK_ESCAPE},        {"insert", VK_INSERT},
      {"delete", VK_DELETE},        {"home", VK_HOME},
      {"end", VK_END},              {"page_up", VK_PRIOR},
      {"prior", VK_PRIOR},          {"page_down", VK_NEXT},
      {"next", VK_NEXT},            {"up", VK_UP},
      {"down", VK_DOWN},            {"left", VK_LEFT},
      {"right", VK_RIGHT},          {"pause", VK_PAUSE},
      {"minus", VK_OEM_MINUS},      {"equal", VK_OEM_PLUS},
      {"comma", VK_OEM_COMMA},      {"period", VK_OEM_PERIOD},
      {"slash", VK_OEM_2},          {"semicolon", VK_OEM_1},
      {"apostrophe", VK_OEM_7},     {"grave", VK_OEM_3},
      {"bracketleft", VK_OEM_4},    {"bracketright", VK_OEM_6},
      {"backslash", VK_OEM_5},
  };
  std::string low = key;
  for (auto& c : low)
    c = (char)tolower((unsigned char)c);
  auto it = names.find(low);
  return it == names.end() ? 0 : it->second;
}
}  // namespace

void WeaselServerApp::JkReloadSettingsGui() {
  std::string path, hotkey;
  RimeApi* api = rime_get_api();
  RimeConfig config = {NULL};
  if (api && api->config_open("weasel", &config)) {
    const char* p = api->config_get_cstring(&config, "jk_settings/path");
    if (p)
      path = p;
    const char* h = api->config_get_cstring(&config, "jk_settings/hotkey");
    if (h)
      hotkey = h;
    api->config_close(&config);
  }
  m_jk_settings_path = path.empty() ? std::wstring() : u8tow(path);

  HWND hwnd = m_server.GetHWnd();
  if (m_jk_hotkey_registered) {
    UnregisterHotKey(hwnd, ID_WEASELTRAY_JK_SETTINGS);
    m_jk_hotkey_registered = false;
  }
  UINT mods = 0;
  UINT vk = hotkey.empty() ? 0 : JkParseHotkey(hotkey, &mods);
  if (vk) {
    m_jk_hotkey_registered = !!RegisterHotKey(hwnd, ID_WEASELTRAY_JK_SETTINGS,
                                              mods | MOD_NOREPEAT, vk);
  }
}

bool WeaselServerApp::JkOpenSettingsGui() {
  fs::path exe = m_jk_settings_path.empty()
                     ? WeaselUserDataPath() / L"settingsGUI" /
                           L"DirectPinyinSettings.exe"
                     : fs::path(m_jk_settings_path);
  if (!fs::exists(exe)) {
    std::wstring msg = L"DirectPinyin settings form not found:\n" +
                       exe.wstring() +
                       L"\n\nSet jk_settings/path in weasel_jk.yaml, then "
                       L"Deploy.";
    MessageBoxW(NULL, msg.c_str(), L"DirectPinyin", MB_OK | MB_ICONWARNING);
    return false;
  }
  return execute(exe, std::wstring());
}
