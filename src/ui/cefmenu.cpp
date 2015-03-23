#include "cefmenu.h"

void InexorCefMenu::InitializeContext()
{
    CreateFunction("show", this);
    CreateFunction("hide", this);
    CreateVariable("visible", true);
}

bool InexorCefMenu::Execute(const CefString& name, CefRefPtr<CefV8Value> object, const CefV8ValueList& arguments, CefRefPtr<CefV8Value>& retval, CefString& exception)
{
    CEF_REQUIRE_RENDERER_THREAD();
    if (name == "show") {
        Show();
        return true;
    } else if (name == "hide") {
        Hide();
        return true;
    }
    return false;
}

bool InexorCefMenu::Get(const CefString& name, const CefRefPtr<CefV8Value> object, CefRefPtr<CefV8Value>& return_value, CefString& exception)
{
    CEF_REQUIRE_RENDERER_THREAD();
    if (name == "visible") {
        return_value = CefV8Value::CreateBool(is_visible);
        return true;
    }
    return false;
}

bool InexorCefMenu::Set(const CefString& name, const CefRefPtr<CefV8Value> object, const CefRefPtr<CefV8Value> value, CefString& exception)
{
    CEF_REQUIRE_RENDERER_THREAD();
    return false;
}

void InexorCefMenu::SetVisibility(bool visible) {
    this->is_visible = visible;
    layer->SetVisibility(visible);
    layer->SetIsAcceptingInput(visible);
    layer->SetFocus(visible);
};
