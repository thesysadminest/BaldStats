#include <wx/app.h> // GUI
#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/grid.h>
#include <wx/sizer.h>

#include <wx/webrequest.h> // HTTPS requests
#include <wx/sstream.h> // reading HTTPS response data
#include <wx/thread.h> // watching logfile update
#include <wx/textfile.h> // reading text file
#include <vector>
#include <wx/filefn.h> //file existence check
#include <wx/filedlg.h> //file dialog


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
	//TODO: more accurate sorting
	if (player1.bwlevel == player2.bwlevel) return player1.fkdr < player2.fkdr;
	return player1.bwlevel < player2.bwlevel;
}


struct AppPreferences {
	wxString API_key = wxEmptyString; //user API key
	wxString logfile_path = wxEmptyString; //path to minecradt logfile
};


class LogFileThread : public wxThread {
public:
	//TODO: re-write using stdio (is faster)
	explicit LogFileThread(wxEvtHandler* parent, wxString received_logfile_path) : wxThread() {
		m_parent = parent;
		logfile_path = received_logfile_path;
		logfile.Open(logfile_path);
		logfile_lines_count = logfile.GetLineCount();
		logfile.Close();
	}

	ExitCode Entry() {
		while (true) {
			logfile.Open(logfile_path);
			if (logfile_lines_count > logfile.GetLineCount()) logfile_lines_count = logfile.GetLineCount();
			while (logfile_lines_count != logfile.GetLineCount()) {
				wxString* temp_line = new wxString();
				*temp_line = logfile.GetLine(logfile_lines_count);
				if (temp_line->size() > 40 && temp_line->substr(11, 28) == "[Client thread/INFO]: [CHAT]" && (*temp_line)[40] != '$') {
					wxCommandEvent evt(wxEVT_LOGFILE_UPDATE_EVENT, wxID_ANY);
					evt.SetClientData(temp_line);
					m_parent->AddPendingEvent(evt);
				}
				else {} //Logline reqected (too short or is message in the chat)
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
	wxString logfile_path;
};


class PreferencesDialog : public wxDialog {
public:
	PreferencesDialog(wxWindow* parent, AppPreferences config)
		: wxDialog(parent, wxID_ANY, "Preferences", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
		CenterOnParent();
		detectpath_button->Bind(wxEVT_BUTTON, &PreferencesDialog::LinkLauncher, this);
		browsepath_button->Bind(wxEVT_BUTTON, &PreferencesDialog::GetPath, this);

		apikey_field->SetValue(config.API_key);
		if (config.API_key != wxEmptyString)
			apikey_field->SetMinSize(apikey_field->GetSizeFromText(config.API_key));
		else apikey_field->SetMinSize(wxSize(200, 23));

		logpath_field->SetValue(config.logfile_path);

		api_sizer->Add(
			new wxStaticText(this, wxID_ANY, "Your API key:"),
			0, wxALL | wxALIGN_CENTER_VERTICAL, 10
		);
		api_sizer->Add(
			apikey_field,
			1, wxALL, 10
		);

		filepath_text_sizer->Add(
			new wxStaticText(this, wxID_ANY, "Minecraft logs path:"),
			0, wxALL | wxALIGN_CENTER_VERTICAL, 10
		);
		filepath_text_sizer->Add(
			logpath_field,
			1, wxALL, 10
		);

		filepath_buttons_sizer->Add(
			detectpath_button,
			0, wxALL, 10
		);
		filepath_buttons_sizer->Add(
			browsepath_button,
			0, wxALL, 10
		);

		filepath_sizer->Add(filepath_text_sizer, 1, wxEXPAND);
		filepath_sizer->Add(
			client_status,
			0, wxALIGN_CENTER
		);
		filepath_sizer->Add(filepath_buttons_sizer, 1, wxALIGN_CENTER);

		dialog_buttons_sizer->Add(
			new wxButton(this, wxID_OK, "Ok"),
			0, wxALL, 10
		);
		dialog_buttons_sizer->Add(
			new wxButton(this, wxID_CANCEL, "Cancel"),
			0, wxALL, 10
		);

		sizer->Add(api_sizer, 0, wxEXPAND);
		sizer->Add(filepath_sizer, 0, wxEXPAND);
		sizer->Add(dialog_buttons_sizer, 1, wxALIGN_RIGHT);
		SetSizerAndFit(sizer);
		LinkLauncher();
	}

	AppPreferences GetConfigs() {
		return AppPreferences{
			apikey_field->GetValue(), logpath_field->GetValue()
		};
	}

private:
	//DECLARE_EVENT_TABLE();
	//TODO: add api key and latest.log path check
	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* api_sizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* filepath_sizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* filepath_text_sizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* filepath_buttons_sizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* dialog_buttons_sizer = new wxBoxSizer(wxHORIZONTAL);

	wxTextCtrl* apikey_field = new wxTextCtrl(this, wxID_ANY);
	wxTextCtrl* logpath_field = new wxTextCtrl(this, wxID_ANY);
	wxStaticText* client_status = new wxStaticText(this, wxID_ANY, "Delete this later"); //delete that later
	wxButton* detectpath_button = new wxButton(this, wxID_ANY, "Detect minecraft launcher");
	wxButton* browsepath_button = new wxButton(this, wxID_ANY, "Browse...");


	void LinkLauncher(bool is_from_button = false) {
		wxString user = wxGetUserName();
		std::pair<wxString, wxString> last = { wxEmptyString, wxEmptyString };
		std::vector <std::pair <wxString, wxString>> client_list = {
			{"C:/Users/" + user + "/AppData/Roaming/.minecraft/logs/latest.log", "Official Launcher"},
			{"C:/Users/" + user + "/.lunarclient/offline/1.8/logs/latest.log", "Lunar Client"},
			{"C:/Users/" + user + "/AppData/Roaming/.minecraft/logs/blclient/chat/latest.log", "Badlion Client"},
			{"C:/Users/" + user + "/AppData/.pvplounge/logs/latest.log", "PVPLounge Client"}
		};
		if (is_from_button) {
			for (std::pair <wxString, wxString> i : client_list) {
				if (wxFileExists(i.first)) {
					if (last.first == wxEmptyString) { last = i; continue; }
					if (wxFileModificationTime(last.first) < wxFileModificationTime(i.first)) last = i;
				}
			}

			if (last.first == wxEmptyString) {
				logpath_field->SetValue(wxEmptyString);
				client_status->SetForegroundColour(wxColour(150, 0, 0));
				client_status->SetLabel("Can't detect your launcher! Try to manually specify the path");
			}
			else {
				logpath_field->SetValue(last.first);
				client_status->SetForegroundColour(wxColour(0, 150, 0));
				client_status->SetLabel("Linked to " + last.second);
			}
		}
		else {
			wxString cur = apikey_field->GetValue();
			bool client_detected = false;
			for (std::pair <wxString, wxString> i : client_list) {
				if (cur == i.first) {
					client_status->SetForegroundColour(wxColour(0, 150, 0));
					client_status->SetLabel("Linked to " + i.second);
					client_detected = true;
					break;
				}
			}
			if (!client_detected) {
				client_status->SetForegroundColour(wxColour(100, 150, 0));
				client_status->SetLabel("Linked to other launcher");
			}
		}
		return;
	}

	void LinkLauncher(wxCommandEvent& e) {
		LinkLauncher(true);
		return;
	}

	void GetPath(wxCommandEvent& e) {
		wxFileDialog* filedlg = new wxFileDialog(this, "Path to minecraft \"latest.log\"...", "C:/Users/" + wxGetUserName() + "/AppData/Roaming",
			wxEmptyString, "Log files (*.log)|*.log", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (filedlg->ShowModal() == wxID_OK) logpath_field->SetValue(filedlg->GetPath());
		LinkLauncher();
	}
};


class Frame : public wxFrame {
public:
	Frame() : wxFrame(nullptr, wxID_ANY, "Bald Stats preview", wxDefaultPosition, wxSize(700, 500), wxDEFAULT_FRAME_STYLE /*| wxSTAY_ON_TOP*/) {
		//button->Bind(wxEVT_BUTTON, &Frame::test);
		/*q1->SetHint("Hypixel API");
		q1->SetValue("f57c9f4a-175b-430c-a261-d8c199abd927");
		q2->SetHint("Nickname");*/

		wxMenu* menuMode = new wxMenu;
		wxMenu* menuEdit = new wxMenu;
		wxMenu* menuHelp = new wxMenu;
		menuMode->AppendRadioItem(1, "BedWars");
		menuMode->AppendRadioItem(2, "SkyWars");
		menuMode->Enable(2, false);
		menuEdit->Append(500, "Preferences");
		wxMenuBar* menuBar = new wxMenuBar;
		menuBar->Append(menuMode, "&Mode");
		menuBar->Append(menuEdit, "&Edit");
		menuBar->Append(menuHelp, "&Help");
		SetMenuBar(menuBar);

		Bind(wxEVT_WEBREQUEST_STATE, [this](wxWebRequestEvent& evt) {
			switch (evt.GetState()) {
			case wxWebRequest::State_Completed:
			{
				wxString* out = new wxString;
				wxStringOutputStream out_stream(&*out);
				evt.GetResponse().GetStream()->Read(out_stream);
				AddPlayer(out);
				ApplogWrite("II", "Request done");
				break;
			}
			// Request failed
			case wxWebRequest::State_Failed:
				ApplogWrite("EE", "Request failed");
				ApplogWrite("EI", evt.GetErrorDescription());
				break;
			}
			});

		//CreateStatusBar();
		//SetStatusText("Welcome to Bald Stats!");

		//text->SetEditable(false);
		//q2->SetEditable(false);
		playerlist.reserve(16);

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

		InitializeFolders();
		CreateLogFileThread();
	}

private:
	wxPanel* panel = new wxPanel(this);
	//wxButton* button = new wxButton(panel, 10, wxT("Press Me!"), { 10, 10 });
	/*wxTextCtrl* text = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, { 50, 50 }, wxSize((400), (300)), wxTE_MULTILINE);
	wxTextCtrl* q1 = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, { 100, 10 }, wxSize((80), (20)));
	wxTextCtrl* q2 = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, { 200, 10 }, wxSize((80), (20)));
	wxTextCtrl* command = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, { 50, 370 }, wxSize((400), (20)));*/
	wxGrid* table = new wxGrid(panel, -1, { 10, 10 }, wxSize(600, 400));

	std::vector <bedwarsPlayer> playerlist;

	LogFileThread* lfThread;
	DECLARE_EVENT_TABLE();

	wxDateTime time; //current time
	wxTextFile applog; //app logfile
	//TODO: normal settings struct
	AppPreferences config;

	void InitializeFolders() {
		bool is_logfile_created_now = false;
		if (!wxFileExists("logfile.txt")) {
			wxTextFile temp_log;
			temp_log.Create("logfile.txt");
			is_logfile_created_now = true;
		}

		applog.Open("logfile.txt");
		ApplogWrite("ST", "");
		ApplogWrite("ST", "Program launched");
		ApplogWrite("ST", "Current date: " + wxNow());
		if (is_logfile_created_now) ApplogWrite("ST", "Applog was missing, created successfully");

		if (!wxFileExists("config.txt")) {
			wxTextFile temp_cfg;
			temp_cfg.Create("config.txt");
			ApplogWrite("ST", "Config file was missing, created successfully. Asking user for settings...");
			ConfigDialog();
			ApplogWrite("ST", "Settings are: API key:" + config.API_key + " , path to logs: " + config.logfile_path);
			/*wxTextFile cfgFile;
			cfgFile.Open("config.txt");
			cfgFile.AddLine("apikey\t" + config.API_key);
			cfgFile.AddLine("gamelogpath\t" + config.logfile_path);
			cfgFile.Write();
			cfgFile.Close();*/
			ApplogWrite("ST", "Preferences are successfully written");
		}
		else {
			wxTextFile cfgFile;
			cfgFile.Open("config.txt");
			unsigned int line_num = 0;
			while (line_num < cfgFile.GetLineCount()) {
				wxString line = cfgFile.GetLine(line_num);
				++line_num;
				wxString param = wxEmptyString, value = wxEmptyString;
				unsigned int c = 0;
				while (c < line.size() && line[c] != '\t') { param.Append(line[c]); ++c; }
				++c;
				if (c > line.size()) {
					//TODO: fatal error: broken cfgfile
				}
				while (c < line.size()) { value.Append(line[c]); ++c; }

				//TODO: check if all data is present and valid
				if (param == "apikey") config.API_key = value;
				else if (param == "gamelogpath") config.logfile_path = value;
			}
			while (!CheckConfigs()) ConfigDialog();
		}
		ApplogWrite("ST", "End of program initialization");
		return;
	}

	void ConfigDialog() {
		PreferencesDialog* dialog = new PreferencesDialog(this, config);
		if (dialog->ShowModal() == wxID_OK)
			UpdateConfigs(dialog->GetConfigs());

		//TODO: else...
		return;
	}

	void ConfigDialog(wxCommandEvent& e) {
		ConfigDialog();
		return;
	}

	void ChangeConfig(wxTextFile& cfgFile, wxString param, wxString value) {
		unsigned int line_num = 0;
		bool param_written = false;
		while (line_num < cfgFile.GetLineCount()) {
			wxString line = cfgFile.GetLine(line_num);
			unsigned int c = 0;
			bool param_found = true;
			while (c < line.size() && line[c] != '\t') {
				if (c >= param.size() || line[c] != param[c]) { param_found = false; break; }
				++c;
			}
			if (!param_found) { ++line_num; continue; }

			param_written = true;
			cfgFile.RemoveLine(line_num);
			cfgFile.InsertLine(param + '\t' + value, line_num);
			break;
		}
		if (!param_written) cfgFile.AddLine(param + '\t' + value);
		return;
	}

	void UpdateConfigs(AppPreferences config_new) {
		wxTextFile cfgFile;
		cfgFile.Open("config.txt");
		if (config.API_key != config_new.API_key) ChangeConfig(cfgFile, "apikey", config_new.API_key);
		if (config.logfile_path != config_new.logfile_path) ChangeConfig(cfgFile, "gamelogpath", config_new.logfile_path);
		cfgFile.Write();
		cfgFile.Close();
		config = config_new;
		return;
	}

	bool CheckConfigs() {
		if (config.API_key == wxEmptyString || config.logfile_path == wxEmptyString) return false;
		if (!wxFileExists(config.logfile_path)) { config.logfile_path = wxEmptyString; return false; }
		return true;
	}

	void ApplogWrite(wxString status, wxString line) {
		//[ST] - program status and user actions, [LL] - logline content, 
		//[II] - information, [WW] - warning, 
		//[EE] - error, [FF] - fatal error, [EI] - useful error information
		time.Set((time_t)wxGetUTCTime());
		applog.AddLine("[" + status + "] [" + wxString::Format("%d", (int)time.GetHour()) +
			":" + wxString::Format("%d", (int)time.GetMinute()) +
			":" + wxString::Format("%d", (int)time.GetSecond()) + "] " + line);
		applog.Write();
		return;
	}

	/*void Fake_Multiple_Request(wxCommandEvent& e) {
		Request("Buchnick");
		Request("RaidFern");
		Request("Jelly_UP");
		Request("akutaagawa");
		Request("Cephalexin");
		Request("CA62851158");
		Request("Minisamufi");
		Request("Lukefain10");
	}*/

	void CreateLogFileThread() {
		ApplogWrite("ST", "Creating a logfile thread");
		lfThread = new LogFileThread(this, config.logfile_path);

		wxThreadError err = lfThread->Create();
		if (err != wxTHREAD_NO_ERROR) {
			ApplogWrite("FE", "Couldn't create thread!");
			ApplogWrite("EI", "Error code: " + wxString::Format("%d", err));
			return;
		}

		err = lfThread->Run();
		if (err != wxTHREAD_NO_ERROR) {
			ApplogWrite("FE", "Couldn't run thread!");
			ApplogWrite("EI", "Error code: " + wxString::Format("%d", err));
			return;
		}

		ApplogWrite("ST", "Logfile thread created successfully!");
		return;
	}

	void ReadLine(wxCommandEvent& evt) {
		wxString line = *(wxString*)evt.GetClientData();
		delete evt.GetClientData();
		ApplogWrite("LL", line);

		std::vector<wxString> parsed;
		wxString temp = wxEmptyString;
		short int linesize = line.size();
		if (*line.end() == '!' || *line.end() == '.') --linesize;
		for (unsigned int i = 40; i < linesize; ++i) {
			if (line[i] == ' ') {
				if (temp[0] != '[' && temp[temp.size() - 1] != ']') parsed.push_back(temp);
				temp = wxEmptyString;
			}
			else temp.append(line[i]);
		}
		if (!temp.empty()) parsed.push_back(temp);

		if (parsed.size() == 3) {
			// player quits the lobby
			if (parsed[1] == "has" && parsed[2] == "quit") {
				ApplogWrite("II", "Player quits the lobby");
				RemovePlayer(parsed[0]);
			}
		}
		else if (parsed.size() == 4) {
			// player joins the party
			if (parsed[1] == "joined" && parsed[3] == "party") {
				ApplogWrite("II", "Player joins the party");
				Request(parsed[0]);
			}
			// you leave the party
			else if (parsed[0] == "You" && parsed[1] == "left") {
				ApplogWrite("II", "You leave the party");
				DisbandParty();
			}
			// player joins the lobby
			else if (parsed[1] == "has" && parsed[2] == "joined") {
				ApplogWrite("II", "Player joins the lobby");
				//Request(parsed[0]);
			}
		}
		else if (parsed.size() == 5) {
			// player leaves the party
			if (parsed[2] == "left" && parsed[4] == "party") {
				ApplogWrite("II", "Player leaves the party");
				RemovePlayer(parsed[0]);
			}
			// you join someone else's party
			else if (parsed[0] == "You" && parsed[4] == "party") {
				ApplogWrite("II", "You join someone else's party");
				Request(parsed[3].substr(0, parsed[3].size() - 2));
			}
		}
		else if (parsed.size() == 7) {
			// player gets removed from the party
			if (parsed[1] == "has" && parsed[3] == "removed") {
				ApplogWrite("II", "Player gets removed from the party");
				RemovePlayer(parsed[0]);
			}
		}
		else if (parsed.size() == 9 || parsed.size() == 12) {
			// party gets disbanded
			if (parsed[1] == "party" && parsed[3] == "disbanded") {
				ApplogWrite("II", "The party gets disbanded");
				DisbandParty();
			}
			// player gets removed from the party
			else if (parsed[1] == "was" && parsed[2] == "removed") {
				ApplogWrite("II", "Player gets removed from the party");
				RemovePlayer(parsed[0]);
			}
		}
		// lobby players list
		if (parsed[0] == "ONLINE:") {
			ApplogWrite("II", "Lobby players list");
			parsed.back() += '.'; //TODO: check if it works
			for (unsigned int i = 1; i < parsed.size(); ++i)
				Request(parsed[i].substr(0, parsed[i].size() - 1));
		}
		// party players list
		else if (parsed.size() > 3 && parsed[0] == "You'll" && parsed[2] == "partying") {
			ApplogWrite("II", "Party players list");
			parsed.back() += '.'; //TODO: check if it works
			for (unsigned int i = 4; i < parsed.size(); ++i)
				Request(parsed[i].substr(0, parsed[i].size() - 1));
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
			ApplogWrite("WW", "Request rejected by server");
			return { "", -1, -1, -1, -1, "" };
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
		ApplogWrite("II", "Player " + res.playername + " was added to playerlist");
		return;
	}

	void RemovePlayer(wxString player) {
		unsigned int i = 0;
		if (playerlist.size() == 0) return;
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
		ApplogWrite("II", "Player " + player + " was removed from playerlist");
		return;
	}

	void DisbandParty() {
		playerlist.clear();
		table->DeleteRows(0, table->GetNumberRows());
		ApplogWrite("II", "Playerlist and table were cleared");
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

	void Request(wxString player) {
		ApplogWrite("II", "Sending a request, player: " + player);
		wxWebRequest request = wxWebSession::GetDefault().CreateRequest(
			this,
			"https://api.hypixel.net/player?key=" + config.API_key + "&name=" + player
		);

		if (!request.IsOk()) {
			ApplogWrite("EE", "Invalid request:");
			ApplogWrite("EI", "https://api.hypixel.net/player?key=" + config.API_key + "&name=" + player);
		}

		request.Start();
		return;
	}
};

//5xx - preferences
BEGIN_EVENT_TABLE(Frame, wxFrame)
EVT_COMMAND(wxID_ANY, wxEVT_LOGFILE_UPDATE_EVENT, Frame::ReadLine)
EVT_MENU(500, Frame::ConfigDialog)
END_EVENT_TABLE()


class Program : public wxApp {
	bool OnInit() override {
		(new Frame())->Show();
		return true;
	}
};


wxIMPLEMENT_APP(Program);
