"""Apply a hash-pinned wx3.2.12 Edge completion-lifetime prerequisite.

Not a Windows WebView backend or a claim of platform acceptance. All COM
completion closures carry numeric IDs, resolved on the GUI thread. Input trees
must be pristine pinned sources (or exactly this patch's output). No network.
"""
import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PUBLIC = "include/wx/msw/webview_edge.h"
PRIVATE = "include/wx/msw/private/webview_edge.h"
SOURCE = "src/msw/webview_edge.cpp"
REGISTRY = "include/wx/msw/private/wxlambda_async_owner.h"
HASHES = {
    PUBLIC: "58e9e037d13c42f389631fb1925d2587449c28424fc709b5bedf6d72b2668c56",
    PRIVATE: "ea6d451e128b686c206b5818590eabe3d7695ad1844b57d71cb8bc321008af0b",
    SOURCE: "c40316f2fa6e69ea6204458184bf0066dde89ee2d55677e6393e2f6a58a7321a",
}

# Each anchor must occur exactly once. Preserve upstream's licence/header.
REPLACEMENTS = {
    PUBLIC: [(
        "    static void MSWSetBrowserExecutableDir(const wxString& path);",
        "    static void MSWSetBrowserExecutableDir(const wxString& path);\n"
        "    // wxLambda pinned completion-lifetime patch, not upstream ABI.\n"
        "    static int MSWGetWXLambdaCompletionGuardVersion();",
    )],
    PRIVATE: [(
        '#include "wx/msw/private/comptr.h"',
        '#include "wx/msw/private/comptr.h"\n#include <cstdint>',
    ), (
        "    wxWebViewEdge* m_ctrl;",
        "    wxWebViewEdge* m_ctrl;\n    std::uint64_t m_wxlCreationId;",
    )],
    SOURCE: [(
        "        if (error == S_OK)\n        {",
        "        if (error == S_OK && result)\n        {",
    ), (
        "    if (SUCCEEDED(errorCode))\n        m_userScriptIds.push_back(id);",
        "    if (SUCCEEDED(errorCode) && id)\n        m_userScriptIds.push_back(id);",
    ), (
        "wxString wxWebViewEdgeImpl::ms_browserExecutableDir;",
        '''wxString wxWebViewEdgeImpl::ms_browserExecutableDir;

#include "wx/msw/private/wxlambda_async_owner.h"
namespace {
wxlambda::AsyncOwnerRegistry<wxWebViewEdgeImpl>& wxlEdgeOwners()
{
    // Process-lifetime non-owning index: callbacks may outlive wx cleanup.
    // Retains no COM objects or windows after the corresponding destructor.
    static auto* registry = new wxlambda::AsyncOwnerRegistry<wxWebViewEdgeImpl>;
    return *registry;
}

HRESULT wxlEdgeCreationError(wxWebViewEdge* ctrl, HRESULT result)
{
    wxWebViewEvent event(wxEVT_WEBVIEW_ERROR, ctrl->GetId(), "about:blank", wxString());
    event.SetEventObject(ctrl);
    event.SetInt(wxWEBVIEW_NAV_ERR_OTHER);
    event.SetString(wxString::Format("WebView2 creation failed (0x%08lx)", result));
    ctrl->GetEventHandler()->AddPendingEvent(event);
    return result;
}
}

int wxWebViewEdge::MSWGetWXLambdaCompletionGuardVersion() { return 1; }''',
    ), (
        "wxWebViewEdgeImpl::wxWebViewEdgeImpl(wxWebViewEdge* webview):\n    m_ctrl(webview)\n{\n\n}",
        "wxWebViewEdgeImpl::wxWebViewEdgeImpl(wxWebViewEdge* webview):\n    m_ctrl(webview),\n    m_wxlCreationId(0)\n{\n"
        "    // Register only after every member has finished construction.\n"
        "    m_wxlCreationId = wxlEdgeOwners().insert(this);\n}",
    ), (
        "wxWebViewEdgeImpl::~wxWebViewEdgeImpl()\n{",
        "wxWebViewEdgeImpl::~wxWebViewEdgeImpl()\n{\n"
        "    wxlEdgeOwners().erase(m_wxlCreationId); // Retire before any native teardown.",
    ), (
        "        m_webView->remove_WebMessageReceived(m_webMessageReceivedToken);\n    }\n}",
        "        m_webView->remove_WebMessageReceived(m_webMessageReceivedToken);\n    }\n"
        "    if (m_webViewController) m_webViewController->Close();\n}",
    ), (
        "    HRESULT hr = wxCreateCoreWebView2EnvironmentWithOptions(",
        "    const auto creationId = m_wxlCreationId;\n"
        "    HRESULT hr = wxCreateCoreWebView2EnvironmentWithOptions(",
    ), (
        "        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(this,\n"
        "            &wxWebViewEdgeImpl::OnEnvironmentCreated).Get());",
        '''        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [creationId](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                try {
                    if (!wxIsMainThread()) return RPC_E_WRONG_THREAD;
                    auto* current = wxlEdgeOwners().find(creationId);
                    return current ? current->OnEnvironmentCreated(result, environment) : S_OK;
                } catch (...) { return E_FAIL; }
            }).Get());''',
    ), (
        '''    HRESULT WXUNUSED(result), ICoreWebView2Environment* environment)
{
    environment->QueryInterface(IID_PPV_ARGS(&m_webViewEnvironment));
    m_webViewEnvironment->CreateCoreWebView2Controller(
        m_ctrl->GetHWND(),
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            this, &wxWebViewEdgeImpl::OnWebViewCreated).Get());
    return S_OK;
}''',
        '''    HRESULT result, ICoreWebView2Environment* environment)
{
    if (FAILED(result) || !environment)
        return wxlEdgeCreationError(m_ctrl, FAILED(result) ? result : E_POINTER);
    HRESULT hr = environment->QueryInterface(IID_PPV_ARGS(&m_webViewEnvironment));
    if (FAILED(hr) || !m_webViewEnvironment)
        return wxlEdgeCreationError(m_ctrl, FAILED(hr) ? hr : E_NOINTERFACE);
    const auto creationId = m_wxlCreationId;
    hr = m_webViewEnvironment->CreateCoreWebView2Controller(
        m_ctrl->GetHWND(),
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [creationId](HRESULT status, ICoreWebView2Controller* controller) -> HRESULT {
                try {
                    if (!wxIsMainThread()) return RPC_E_WRONG_THREAD;
                    auto* current = wxlEdgeOwners().find(creationId);
                    if (current) return current->OnWebViewCreated(status, controller);
                    if (controller) controller->Close(); // Late unadopted native resource.
                    return S_OK;
                } catch (...) { return E_FAIL; }
            }).Get());
    return FAILED(hr) ? wxlEdgeCreationError(m_ctrl, hr) : hr;
}''',
    ), (
        '''    if (FAILED(result))
    {
        wxLogApiError("WebView2::WebViewCreated", result);
        return result;
    }''',
        '''    if (FAILED(result) || !webViewController)
    {
        if (webViewController) webViewController->Close();
        return wxlEdgeCreationError(m_ctrl, FAILED(result) ? result : E_POINTER);
    }''',
    ), (
        '''    if (FAILED(hr))
    {
        wxLogApiError("WebView2::WebViewCreated (get_CoreWebView2)", hr);
        return result;
    }''',
        '''    if (FAILED(hr) || !baseWebView)
    {
        // Failed native view retrieval: no controller may survive rollback.
        webViewController->Close();
        return wxlEdgeCreationError(m_ctrl, FAILED(hr) ? hr : E_POINTER);
    }''',
    ), (
        '''    if (FAILED(hr))
    {
        wxLogApiError("WebView2::WebViewCreated (QueryInterface)", hr);
        return result;
    }''',
        '''    if (FAILED(hr) || !m_webView)
    {
        // Failed interface query: no controller may survive rollback.
        webViewController->Close();
        return wxlEdgeCreationError(m_ctrl, FAILED(hr) ? hr : E_NOINTERFACE);
    }''',
    ), (
        "    // Start script execution\n    HRESULT executionResult",
        "    const auto creationId = m_impl->m_wxlCreationId;\n"
        "    // Start script execution\n    HRESULT executionResult",
    ), (
        '''        [this, clientData](HRESULT error, PCWSTR result) -> HRESULT
    {
        // Handle script execution callback''',
        '''        [creationId, clientData](HRESULT error, PCWSTR result) -> HRESULT
    {
        try {
        if (!wxIsMainThread()) return RPC_E_WRONG_THREAD;
        auto* current = wxlEdgeOwners().find(creationId);
        if (!current) return S_OK;
        // Handle script execution callback''',
    ), (
        "            SendScriptResult(clientData, success, scriptExtractedOutput);",
        "            current->m_ctrl->SendScriptResult(clientData, success, scriptExtractedOutput);",
    ), (
        '''            SendScriptResult(clientData, false, wxString::Format("%s (0x%08lx)",
                wxSysErrorMsgStr(error), error));''',
        '''            current->m_ctrl->SendScriptResult(clientData, false, wxString::Format("%s (0x%08lx)",
                wxSysErrorMsgStr(error), error));''',
    ), (
        "        return S_OK;\n    }).Get());\n    if (FAILED(executionResult))",
        "        return S_OK;\n        } catch (...) { return E_FAIL; }\n    }).Get());\n    if (FAILED(executionResult))",
    ), (
        '''            Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(m_impl,
            &wxWebViewEdgeImpl::OnAddScriptToExecuteOnDocumentedCreatedCompleted).Get());''',
        '''            Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
                [creationId](HRESULT result, LPCWSTR id) -> HRESULT {
                    try {
                        if (!wxIsMainThread()) return RPC_E_WRONG_THREAD;
                        auto* current = wxlEdgeOwners().find(creationId);
                        return current ? current->OnAddScriptToExecuteOnDocumentedCreatedCompleted(result, id) : S_OK;
                    } catch (...) { return E_FAIL; }
                }).Get());''',
    ), (
        "bool wxWebViewEdge::AddUserScript(const wxString& javascript,\n    wxWebViewUserScriptInjectionTime injectionTime)\n{",
        "bool wxWebViewEdge::AddUserScript(const wxString& javascript,\n    wxWebViewUserScriptInjectionTime injectionTime)\n{\n"
        "    const auto creationId = m_impl->m_wxlCreationId;",
    )],
}


# Native event delegates also resolve the numeric identity, rather than relying
# solely on token removal to prevent invocation of a freed C++ delegate target.
REPLACEMENTS[SOURCE].append((
    "    // Connect and handle the various WebView events",
    "    const auto creationId = m_wxlCreationId;\n    // Connect and handle the various WebView events",
))
for interface, method, argument in (
    ("NavigationStarting", "OnNavigationStarting", "ICoreWebView2NavigationStartingEventArgs"),
    ("SourceChanged", "OnSourceChanged", "ICoreWebView2SourceChangedEventArgs"),
    ("NavigationCompleted", "OnNavigationCompleted", "ICoreWebView2NavigationCompletedEventArgs"),
    ("NewWindowRequested", "OnNewWindowRequested", "ICoreWebView2NewWindowRequestedEventArgs"),
    ("DocumentTitleChanged", "OnDocumentTitleChanged", "IUnknown"),
    ("DOMContentLoaded", "OnDOMContentLoaded", "ICoreWebView2DOMContentLoadedEventArgs"),
    ("ContainsFullScreenElementChanged", "OnContainsFullScreenElementChanged", "IUnknown"),
    ("WebMessageReceived", "OnWebMessageReceived", "ICoreWebView2WebMessageReceivedEventArgs"),
):
    delegate = f"ICoreWebView2{interface}EventHandler"
    REPLACEMENTS[SOURCE].append((
        f"Callback<{delegate}>(\n            this, &wxWebViewEdgeImpl::{method}).Get()",
        f'''Callback<{delegate}>(
            [creationId](ICoreWebView2* sender, {argument}* args) -> HRESULT {{
                try {{
                    if (!wxIsMainThread()) return RPC_E_WRONG_THREAD;
                    auto* current = wxlEdgeOwners().find(creationId);
                    return current ? current->{method}(sender, args) : S_OK;
                }} catch (...) {{ return E_FAIL; }}
            }}).Get()''',
    ))

def transform(path, text):
    for old, new in REPLACEMENTS[path]:
        if text.count(old) != 1:
            raise ValueError(f"{path}: expected exactly one pinned patch anchor: {old[:75]!r}")
        text = text.replace(old, new, 1)
    return text


def prepare(root):
    """Validate every input before returning any writes; reject mixed/stale trees."""
    writes = {}
    states = []
    for name, digest in HASHES.items():
        path = root / name
        raw = path.read_bytes()
        # Git on Windows may check out CRLF; hashes are over canonical LF UTF-8.
        original = raw.replace(b"\r\n", b"\n")
        text = original.decode("utf-8")
        if hashlib.sha256(original).hexdigest() == digest:
            writes[path] = transform(name, text).encode("utf-8")
            states.append("pristine")
        else:
            # Derive pristine source by reversing every exact replacement.
            restored = text
            for old, new in reversed(REPLACEMENTS[name]):
                if restored.count(new) != 1:
                    raise ValueError(f"{name}: not pristine pinned wx3.2.12 or exact patched output")
                restored = restored.replace(new, old, 1)
            if hashlib.sha256(restored.encode("utf-8")).hexdigest() != digest:
                raise ValueError(f"{name}: upstream hash mismatch")
            states.append("patched")
    helper = (ROOT / "native/async-owner-registry.h").read_bytes().replace(b"\r\n", b"\n")
    destination = root / REGISTRY
    if destination.exists() and destination.read_bytes().replace(b"\r\n", b"\n") != helper:
        raise ValueError("Existing asynchronous owner helper differs; refusing overwrite")
    if len(set(states)) != 1 or (states[0] == "patched" and not destination.exists()):
        raise ValueError("Partially patched wx tree; restore pinned sources and retry")
    if states[0] == "patched":
        return {}
    writes[destination] = helper
    return writes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("wx_source", type=Path)
    parser.add_argument("--check", action="store_true", help="validate pristine or exact patched tree without writing")
    arguments = parser.parse_args()
    try:
        writes = prepare(arguments.wx_source)
        if not arguments.check:
            for path, data in writes.items():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
        print(f"PASS: pinned Edge completion patch {'validated' if arguments.check else 'applied'}; {len(writes)} files {'would change' if arguments.check else 'changed'}")
    except (OSError, ValueError, UnicodeError) as error:
        parser.exit(1, f"Edge patch rejected: {error}\n")


if __name__ == "__main__":
    main()
