#include <wx/app.h> // GUI
#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/grid.h>
#include <wx/webrequest.h> // HTTPS requests
#include <wx/sstream.h> // reading HTTPS response data
#include <wx/thread.h> // watching logfile update
#include <wx/textfile.h> // reading text file
#include <vector>


DECLARE_LOCAL_EVENT_TYPE(wxEVT_LOGFILE_UPDATE_EVENT, -1)
DEFINE_LOCAL_EVENT_TYPE(wxEVT_LOGFILE_UPDATE_EVENT)


struct bedwarsPlayer {
	wxString playername;
	int bwlevel;
	float fkdr;
	int finalk, finald;
	wxString uuid;
};

bool operator<(bedwarsPlayer player1, bedwarsPlayer player2) {
	// -*-*-*-*-
	if (player1.bwlevel == player2.bwlevel) return player1.fkdr < player2.fkdr;
	return player1.bwlevel < player2.bwlevel;
}


class LogFileThread : public wxThread {
public:
	explicit LogFileThread(wxEvtHandler* parent) : wxThread() {
		m_parent = parent;
		logfile.Open(logfile_path);
		logfile_lines_count = logfile.GetLineCount();
		logfile.Close();
	}

	ExitCode Entry() {
		while (true) {
			logfile.Open(logfile_path);
			while (logfile_lines_count != logfile.GetLineCount()) {
				wxString* temp_line = new wxString();
				*temp_line = logfile.GetLine(logfile_lines_count);
				if (temp_line->size() > 40 && temp_line->substr(11, 28) == "[Client thread/INFO]: [CHAT]" && (*temp_line)[40] != '$') {
					wxCommandEvent evt(wxEVT_LOGFILE_UPDATE_EVENT, wxID_ANY);
					evt.SetClientData(temp_line);
					m_parent->AddPendingEvent(evt);
				}
				else {
					wxLogInfo("Logline rejected");
				}
				++logfile_lines_count;
			}
			logfile.Close();
			wxMilliSleep(200);
		}
		return ExitCode(nullptr);
	}

private:
	wxEvtHandler* m_parent;
	int logfile_lines_count;
	wxTextFile logfile;
	wxString logfile_path = "D:/Programming/C++/BaldStats/a.txt";
};


class Frame : public wxFrame {
public:
	Frame() : wxFrame(nullptr, wxID_ANY, "Testing", wxDefaultPosition, wxSize(1400, 500), wxDEFAULT_FRAME_STYLE /*| wxSTAY_ON_TOP*/) {
		//button->Bind(wxEVT_BUTTON, &Frame::, this);
		q1->SetHint("Hypixel API");
		q1->SetValue("f57c9f4a-175b-430c-a261-d8c199abd927");
		q2->SetHint("Nickname");
		wxMessageDialog(this, "Please, close this app. It doesn't works properly now", "Important!", wxOK | wxCENTRE | wxICON_ERROR).ShowModal();

		wxMenu* menuMode = new wxMenu;
		menuMode->AppendRadioItem(1, "BedWars");
		menuMode->AppendRadioItem(2, "SkyWars");
		menuMode->Enable(2, false);
		wxMenuBar* menuBar = new wxMenuBar;
		menuBar->Append(menuMode, "&Mode");
		SetMenuBar(menuBar);

		Bind(wxEVT_WEBREQUEST_STATE, [this](wxWebRequestEvent& evt) {
			switch (evt.GetState()) {
			case wxWebRequest::State_Completed:
			{
				wxString* out = new wxString;
				wxStringOutputStream out_stream(&*out);
				evt.GetResponse().GetStream()->Read(out_stream);
				AddPlayer(out);
				wxLogInfo("Request done");
				break;
			}
			// Request failed
			case wxWebRequest::State_Failed:
				wxLogError(evt.GetErrorDescription());
				break;
			}
			});

		//CreateStatusBar();
		//SetStatusText("Welcome to Bald Stats!");

		//text->SetEditable(false);
		//q2->SetEditable(false);
		playerlist.reserve(16);
		CreateLogFileThread();

		table->CreateGrid(0, 5);
		table->SetDefaultCellFont(wxFont(14, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
		table->SetDefaultRowSize(28);
		table->SetColLabelValue(0, "Name");
		table->SetColLabelValue(1, "Level");
		table->SetColLabelValue(2, "FKDR");
		table->SetColLabelValue(3, "Final kills");
		table->SetColLabelValue(4, "Final deaths");
		table->SetRowLabelSize(20);
		table->SetDefaultCellAlignment(wxALIGN_CENTRE, wxALIGN_CENTRE);
		table->SetDefaultCellOverflow(false);
	}
	
private:
	wxPanel* panel = new wxPanel(this);
	wxButton* button = new wxButton(panel, wxID_ANY, wxT("Press Me!"), { 10, 10 });
	wxTextCtrl* text = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, { 50, 50 }, wxSize((400), (300)), wxTE_MULTILINE);
	wxTextCtrl* q1 = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, { 100, 10 }, wxSize((80), (20)));
	wxTextCtrl* q2 = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, { 200, 10 }, wxSize((80), (20)));
	wxTextCtrl* command = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, { 50, 370 }, wxSize((400), (20)));
	wxGrid* table = new wxGrid(panel, -1, {500, 10}, wxSize(800, 400));

	std::vector <bedwarsPlayer> playerlist;

	LogFileThread* m_pThread;
	DECLARE_EVENT_TABLE();

	void Fake_Multiple_Request(wxCommandEvent& e) {
		Request("Buchnick");
		Request("RaidFern");
		Request("Jelly_UP");
		Request("akutaagawa");
		Request("Cephalexin");
		Request("CA62851158");
		Request("Minisamufi");
		Request("Lukefain10");
	}

	void CreateLogFileThread() {
		m_pThread = new LogFileThread(this);
		wxThreadError err = m_pThread->Create();

		if (err != wxTHREAD_NO_ERROR) {
			wxMessageBox(_("Couldn't create thread!"));
			return;
		}

		err = m_pThread->Run();

		if (err != wxTHREAD_NO_ERROR) {
			wxMessageBox(_("Couldn't run thread!"));
			return;
		}
	}

	void ReadLine(wxCommandEvent& evt) {
		wxString line = *(wxString*)evt.GetClientData();
		delete evt.GetClientData();
		wxLogInfo(line);

		std::vector<wxString> parsed;
		wxString temp = wxEmptyString;
		for (unsigned int i = 40; i < line.size(); ++i) {
			if (line[i] == ' ') {
				if (temp[0] != '[' && temp[temp.size() - 1] != ']') parsed.push_back(temp);
				temp = wxEmptyString;
			}
			else temp.append(line[i]);
		}
		if (!temp.empty()) parsed.push_back(temp);
		//for (auto i : parsed) wxLogInfo(i);

		if (parsed.size() == 3) {
			// player quits the lobby
			if (parsed[1] == "has" && parsed[2] == "quit!") {
				wxLogInfo("player quits the lobby");
				RemovePlayer(parsed[0]);
			}
		}
		else if (parsed.size() == 4) {
			// player joins the party
			if (parsed[1] == "joined" && parsed[3] == "party") {
				wxLogInfo("player joins the party");
				Request(parsed[0]);
			}
			// you leave the party
			else if (parsed[0] == "You" && parsed[1] == "left") {
				wxLogInfo("you leave the party");
				//DisbandParty()
			}
			// player joins the lobby
			else if (parsed[1] == "has" && parsed[2] == "joined") {
				wxLogInfo("player joins the lobby");
				//Request(parsed[0]);
			}
		}
		else if (parsed.size() == 5) {
			// player leaves the party
			if (parsed[2] == "left" && parsed[4] == "party") {
				wxLogInfo("player leaves the party");
				RemovePlayer(parsed[0]);
			}
			// you join someone else's party
			else if (parsed[0] == "You" && parsed[4] == "party!") {
				wxLogInfo("you join someone else's party");
				Request(parsed[3].substr(0, parsed[3].size() - 2));
			}
		}
		else if (parsed.size() == 7) {
			// player gets removed from the party
			if (parsed[1] == "has" && parsed[3] == "removed") {
				wxLogInfo("player gets removed from the party");
				RemovePlayer(parsed[0]);
			}
		}
		else if (parsed.size() == 9 || parsed.size() == 12) {
			// party gets disbanded
			if (parsed[1] == "party" && parsed[3] == "disbanded") {
				wxLogInfo("the party gets disbanded");
				//DisbandParty()
			}
		}
		// lobby players list
		else if (parsed[0] == "ONLINE:") {
			wxLogInfo("lobby players list");
		}
		// party players list
		else if (parsed[0] == "You'll" && parsed[2] == "partying") {
			wxLogInfo("party players list");
		}
		return;
	}

	int convertStringToInt(wxString s) {
		int n = 0;
		for (unsigned int i = 0; i < s.size(); ++i) {
			n *= 10;
			n += int((char)s[i] - '0');
		}
		return n;
	}

	bedwarsPlayer GetPlayerStats(wxString* json_ptr) {
		int success_pos = JsonGetPos(0, "success", json_ptr);
		int player_pos = JsonGetPos(0, "player", json_ptr);
		if (JsonGetValue(success_pos, json_ptr) != "true" || (*json_ptr)[player_pos + 1] != '{') {
			wxLogInfo("request rejected");
			return {"", -1, -1, -1, -1, ""};
		}
		int displayname_pos = JsonGetPos(player_pos, "displayname", json_ptr);
		wxString displayname = JsonGetValue(displayname_pos, json_ptr);
		wxString uuid = JsonGetValue(JsonGetPos(player_pos, "uuid", json_ptr), json_ptr);
		int achievements_pos = JsonGetPos(player_pos, "achievements", json_ptr);
		wxString s_bwlevel = JsonGetValue(JsonGetPos(achievements_pos, "bedwars_level", json_ptr), json_ptr);
		int stats_pos = JsonGetPos(player_pos, "stats", json_ptr);
		int bedwars_pos = JsonGetPos(stats_pos, "Bedwars", json_ptr);
		wxString s_finalk = JsonGetValue(JsonGetPos(bedwars_pos, "final_kills_bedwars", json_ptr), json_ptr);
		wxString s_finald = JsonGetValue(JsonGetPos(bedwars_pos, "final_deaths_bedwars", json_ptr), json_ptr);
		int bwlevel = convertStringToInt(s_bwlevel), finalk = convertStringToInt(s_finalk), finald = convertStringToInt(s_finald);
		float fkdr;
		if (finald == 0) fkdr = 1;
		else fkdr = float(finalk) / finald;
		/*wxLogMessage(displayname);
		wxLogMessage(uuid);
		wxLogMessage(s_bwlevel);
		wxLogMessage(s_finalk);
		wxLogMessage(s_finald);*/
		return { displayname, bwlevel, fkdr, finalk, finald, uuid };
	}

	void AddPlayer(wxString* response) {
		bedwarsPlayer res = GetPlayerStats(response);
		delete response;
		if (res.playername == "") return;
		unsigned int i = 0;
		while (i < playerlist.size()) {
			if (playerlist[i] < res) break;
			++i;
		}
		playerlist.push_back({});

		table->InsertRows(i);
		table->SetCellValue(wxGridCellCoords(i, 0), res.playername);
		table->SetCellValue(wxGridCellCoords(i, 1), wxString::Format("%d", res.bwlevel));
		table->SetCellValue(wxGridCellCoords(i, 2), wxString::Format("%f", res.fkdr));
		table->SetCellValue(wxGridCellCoords(i, 3), wxString::Format("%d", res.finalk));
		table->SetCellValue(wxGridCellCoords(i, 4), wxString::Format("%d", res.finald));
		//table->Update();

		bedwarsPlayer prev = res;
		while (i < playerlist.size()) {
			std::swap(prev, playerlist[i]);
			++i;
		}
		return;
	}

	void RemovePlayer(wxString player) {
		unsigned int i = 0;
		while (true) {
			if (playerlist[i].playername == player) break;
			++i;
			if (i >= playerlist.size()) return;
		}

		table->DeleteRows(i);
		if (i != playerlist.size() - 1) {
			++i;
			while (i < playerlist.size()) {
				playerlist[i - 1] = playerlist[i];
				++i;
			}
		}
		playerlist.pop_back();
		return;
	}

	// returns index of ":" in " "key":"value {} []" "
	// if not exist, returns -1 or runtime error
	int JsonGetPos(int start, wxString key, wxString* json_ptr) {
		wxString json = *json_ptr;
		int level = 0;
		int index = start;
		if (json[index + 1] == '{') ++index;
		while (true) {
			index += 2;
			bool flag = true;
			for (unsigned int i2 = 0; i2 < key.size(); ++i2) {
				if (json[index] != key[i2] || json[index] == '"') { flag = false; break; }
				++index;
			}
			if (flag && json[index] == '"') return ++index;
			if (json[index] != '"') while (json[index] != '"') ++index;
			index += 2;

			if (json[index] == '[') { while (json[index] != ']') ++index; ++index; }
			else if (json[index] == '"') {
				++index;
				while (json[index] != '"') ++index;
				++index;
			}
			else if (json[index] == '{') {
				++level;
				while (level != 0) {
					++index;
					if (json[index] == '{') ++level;
					else if (json[index] == '}') --level;
				}
				++index;
			}
			else while (json[index] != ',' && json[index] != '}') ++index;
			if (json[index] == '}') return -1;
		}
		return -1;
	}

	// returns value after ":" index in " "key":"value" "
	wxString JsonGetValue(int index, wxString* json_ptr) {
		wxString json = *json_ptr;
		if (index == -1) return wxEmptyString;
		wxString res = wxEmptyString;
		++index;
		if (json[index] == '"') {
			++index;
			while (json[index] != '"') {
				res.append(json[index]);
				++index;
			}
		}
		else while (json[index] != ',' && json[index] != '}') {
			res.append(json[index]);
			++index;
		}
		return res;
	}

	void Request(wxString qqq2) {
		wxLogInfo("Requesting " + qqq2);
		wxWebRequest request = wxWebSession::GetDefault().CreateRequest(
			this,
			"https://api.hypixel.net/player?key=" + q1->GetValue() + "&name=" + qqq2
		);

		if (!request.IsOk()) {
			wxLogError("Check request");
		}

		request.Start();
	}
};


BEGIN_EVENT_TABLE(Frame, wxFrame)
EVT_COMMAND(wxID_ANY, wxEVT_LOGFILE_UPDATE_EVENT, Frame::ReadLine)
END_EVENT_TABLE()


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