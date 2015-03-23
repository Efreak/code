#ifndef _CEF_BROWSER_H
#define _CEF_BROWSER_H

#include "cefclienthandler.h"
#include "include/cef_app.h"

class InexorCefApp : public CefApp,
                     public CefBrowserProcessHandler
{

    public:
	    InexorCefApp(std::string url);
	    InexorCefApp(std::string url, int x, int y, int width, int height);

	    virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() { return this; }
	    virtual void OnContextInitialized();
	    CefBrowserSettings GetBrowserSettings() { return browser_settings; };
	    CefRefPtr<InexorCefClientHandler> GetClientHandler() { return client_handler; };
	    CefRefPtr<CefBrowser> GetBrowser() { return browser; };
	    CefRefPtr<InexorCefRenderHandler> GetRenderHandler() { return render_handler; };


	private:
	    std::string url;
	    CefWindowInfo window_info;
        CefRefPtr<CefBrowser> browser;
	    CefRefPtr<InexorCefClientHandler> client_handler;
	    CefBrowserSettings browser_settings;
	    CefRefPtr<InexorCefRenderHandler> render_handler;

	    // Include the default reference counting implementation.
	    IMPLEMENT_REFCOUNTING(InexorCefApp);

};

#endif  // _CEF_BROWSER_H
