#include "ceflayer.h"

InexorCefLayer::InexorCefLayer(std::string name, int x, int y, int width, int height, std::string url)
    : name(name),
      url(url),
      is_visible(false),
      is_accepting_input(false),
      has_focus(false)
{
    window_info.x = x;
    window_info.y = y;
    window_info.width = width;
    window_info.height = height;
    render_handler = new InexorCefRenderHandler(true, x, y, width, height);
    client_handler = new InexorCefClientHandler(render_handler);
    browser = CefBrowserHost::CreateBrowserSync(window_info, client_handler.get(), url, browser_settings, NULL);
    if (browser.get()) {
        logoutf("init: cef: created layer \"%s\"", name.c_str());
        browser->GetHost()->SendFocusEvent(has_focus);
    }
}

void InexorCefLayer::SetVisibility(bool is_visible)
{
	this->is_visible = is_visible;
	browser->GetHost()->SetWindowVisibility(is_visible);
	browser->GetHost()->WasHidden(!is_visible);
}

void InexorCefLayer::SetFocus(bool has_focus)
{
	this->has_focus = has_focus;
	browser->GetHost()->SendFocusEvent(has_focus);
}

void InexorCefLayer::SetIsAcceptingInput(bool is_accepting_input)
{
	this->is_accepting_input = is_accepting_input;
}

void InexorCefLayer::Destroy()
{
    logoutf("InexorCefLayer::Destroy()");
    client_handler->DoClose(browser);
}

CefRefPtr<InexorCefClientHandler> InexorCefLayer::GetClientHandler()
{
    return client_handler;
}

CefRefPtr<InexorCefRenderHandler> InexorCefLayer::GetRenderHandler()
{
    return render_handler;
}
