#include "cefapp.h"

InexorCefApp::InexorCefApp(int width, int height)
{
    SetScreenSize(width, height);
    mouse.Show();
}

InexorCefLayer* InexorCefApp::CreateLayer(std::string name, int x, int y, int width, int height, std::string url)
{
	InexorCefLayer* layer = new InexorCefLayer(name, x, y, width, height, url);
	layers.push_back(layer);
	return layer;
}

InexorCefLayer* InexorCefApp::GetLayer(std::string name)
{
    return NULL;
}

void InexorCefApp::SetScreenSize(int width, int height) {
    this->width = width;
    this->height = height;
    mouse.SetMax(width, height);
}

void InexorCefApp::RenderLayer(std::string name)
{
	GetLayer(name)->GetRenderHandler()->Render();
}

void InexorCefApp::Render()
{
	CEF_REQUIRE_UI_THREAD();
    for(std::list<InexorCefLayer*>::iterator it = layers.begin(); it != layers.end(); ++it)
    {
    	InexorCefLayer* layer = (*it);
    	if (layer->IsVisible()) {
        	layer->GetRenderHandler()->Render();
    	}
    }
    // Render mouse in front
    mouse.Render();
}

void InexorCefApp::SendKeyEvent(CefKeyEvent event)
{
    for(std::list<InexorCefLayer*>::iterator it = layers.begin(); it != layers.end(); ++it)
    {
        InexorCefLayer* layer = (*it);
        if (layer->IsVisible()) {
            layer->GetBrowser()->GetHost()->SendKeyEvent(event);
        }
    }
}

void InexorCefApp::SendMouseClickEvent(const CefMouseEvent& event, CefBrowserHost::MouseButtonType type, bool mouseUp, int clickCount)
{
    for(std::list<InexorCefLayer*>::iterator it = layers.begin(); it != layers.end(); ++it)
    {
        InexorCefLayer* layer = (*it);
        if (layer->IsVisible()) {
            layer->GetBrowser()->GetHost()->SendMouseClickEvent(event, type, mouseUp, clickCount);
        }
    }
}

void InexorCefApp::SendMouseMoveEvent(const CefMouseEvent& event, bool mouseLeave)
{
    for(std::list<InexorCefLayer*>::iterator it = layers.begin(); it != layers.end(); ++it)
    {
        InexorCefLayer* layer = (*it);
        if (layer->IsVisible()) {
            layer->GetBrowser()->GetHost()->SendMouseMoveEvent(event, mouseLeave);
        }
    }
    // Set mouse pointer position
    mouse.Update(event);
}

void InexorCefApp::SendMouseWheelEvent(const CefMouseEvent& event, int deltaX, int deltaY)
{
    for(std::list<InexorCefLayer*>::iterator it = layers.begin(); it != layers.end(); ++it)
    {
        InexorCefLayer* layer = (*it);
        if (layer->IsVisible()) {
            layer->GetBrowser()->GetHost()->SendMouseWheelEvent(event, deltaX, deltaY);
        }
    }
}

void InexorCefApp::OnContextInitialized()
{
    CEF_REQUIRE_UI_THREAD();
    logoutf("init: cef: create layers");
    InexorCefLayer* layer2 = CreateLayer("menu-navigation", 0, 0, width, height, "http://gitdemo.inexor.org/menuprototype/");
    layer2->SetVisibility(true);
    layer2->SetIsAcceptingInput(true);
    layer2->SetFocus(true);
    InexorCefLayer* layer3 = CreateLayer("inexor-object", 0, 0, width, height, "http://gitdemo.inexor.org/inexor-object/");
    layer3->SetVisibility(true);
    layer3->SetIsAcceptingInput(true);
    layer3->SetFocus(true);
}

void InexorCefApp::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> browser_context)
{
    logoutf("Injecting inexor object into javascript context");
    // only inject context if...
    browser_context->GetGlobal()->SetValue("inexor", context.GetContext(), V8_PROPERTY_ATTRIBUTE_NONE);
}
