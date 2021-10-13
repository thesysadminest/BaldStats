#include <wx/app.h>
#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/button.h>
#include <wx/textctrl.h>

#include <wx/sstream.h>
#include <wx/webrequest.h>

class Frame : public wxFrame {
public:
	Frame() : wxFrame(nullptr, wxID_ANY, "Testing", wxDefaultPosition, wxSize(500, 500)) {
		button->Bind(wxEVT_BUTTON, &Frame::test, this);
		q1->SetHint("Hypixel API");
		q2->SetHint("Nickname");

	}

private:
	wxPanel* panel = new wxPanel(this);
	wxButton* button = new wxButton(panel, wxID_ANY, wxT("Press Me!"), { 10, 10 });
	wxTextCtrl* text = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, { 50, 50 }, wxSize((400), (300)), wxTE_MULTILINE);
	wxTextCtrl* q1 = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, { 100, 10 }, wxSize((80), (20)));
	wxTextCtrl* q2 = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, { 200, 10 }, wxSize((80), (20)));
	void test(wxCommandEvent& e) {
		//wxLogInfo(q1->GetValue());
		text->SetValue(wxEmptyString);
		Request();
	}
	void Request() {
		wxWebRequest request = wxWebSession::GetDefault().CreateRequest(
			this,
			"https://api.hypixel.net/player?key=" + q1->GetValue() + "&name=" + q2->GetValue()
		);

		if (!request.IsOk()) {
			wxLogError("Check request");
		}

		Bind(wxEVT_WEBREQUEST_STATE, [this](wxWebRequestEvent& evt) {
			switch (evt.GetState()) {
			case wxWebRequest::State_Completed:
			{
				wxString out = wxEmptyString;
				wxStringOutputStream out_stream(&out);
				evt.GetResponse().GetStream()->Read(out_stream);
				//wxLogInfo(out);
				text->WriteText(out);
				break;
			}
			// Request failed
			case wxWebRequest::State_Failed:
				wxLogError(evt.GetErrorDescription());
				break;
			}
			});
		// Start the request
		request.Start();
	}


};

class Program : public wxApp {
	bool OnInit() override {
		(new Frame())->Show();
		return true;
	}
};

wxIMPLEMENT_APP(Program);


//	REQUESTS
		//	wxString request_str = "key=f57c9f4a-175b-430c-a261-d8c199abd927&name=usainbald";
		//	//http.SetHeader("content-type", "application/x-www-form-urlencoded");
		//	//http.SetHeader("content-length", wxString::Format(wxT("%d"), request_str.Length()));
		//	//http.SetHeader("accept", "*/*");
		//	//http.SetHeader("user-agent", "runscope/0.1");
		//	http.SetTimeout(10);
		//	http.SetPostText("application/x-www-form-urlencoded", request_str);
		//	//http.SetMethod("POST");
		//	//wxLogInfo(http.GetHeader(_T("connection")));
		//	http.Connect(_T("api.hypixel.net"), 443);
		//	//http.Connect(_T("www.snee.com"));
		//	//wxSleep(1);
		//	//if (!wxApp::IsMainLoopRunning()) wxLogError("AAAA");
		//	wxInputStream* stream = http.GetInputStream(_T("/player"));
		//	wxLogInfo(wxString(_T(" GetInputStream: ")) << http.GetResponse() << _T("-") << http.GetError());
		//	//wxSleep(1);
		//	if (http.GetError() == wxPROTO_NOERR) {
		//		wxLogInfo(http.GetHeader("location"));
		//		wxString res = wxEmptyString;
		//		wxStringOutputStream output_res(&res);
		//		stream->Read(output_res);
		//		text->AppendText(res);
		//		//wxLogInfo(res);
		//		wxLogInfo(wxString(_T(" returned document length: ")) << res.Length());
		//	}
		//	else {
		//		switch (http.GetError()) {
		//		case wxPROTO_ABRT: {wxLogError("wxPROTO_ABRT"); break; }
		//		case wxPROTO_CONNERR: {wxLogError("wxPROTO_CONNERR"); break; }
		//		case wxPROTO_INVVAL: {wxLogError("wxPROTO_INVVAL"); break; }
		//		case wxPROTO_NETERR: {wxLogError("wxPROTO_NETERR");  break; }
		//		case wxPROTO_NOFILE: {wxLogError("wxPROTO_NOFILE"); break; }
		//		case wxPROTO_NOHNDLR: {wxLogError("wxPROTO_NOHNDLR"); break; }
		//		case wxPROTO_PROTERR: {wxLogError("wxPROTO_PROTERR"); break; }
		//		case wxPROTO_RCNCT: {wxLogError("wxPROTO_RCNCT"); break; }
		//		case wxPROTO_STREAMING: {wxLogError("wxPROTO_STREAMING"); break; }
		//		}
		//	}
		//	wxDELETE(stream);
		//	http.Close();