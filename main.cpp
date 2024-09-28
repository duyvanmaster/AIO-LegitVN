#include <Windows.h>
#include <string>
#include "utils.hpp"
#include "skStr.h"
#include <iostream>
#include <cstdlib>
#include <urlmon.h>
#include <dxgi.h>
#include <cstdio>
#include <shlwapi.h>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <algorithm>
#include <tchar.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <direct.h>
#include <algorithm>
#include <filesystem>
#include <unrar.h>
#include <curl/curl.h>


#pragma comment(lib, "dxgi.lib")
namespace fs = std::filesystem;
using namespace std;
#pragma comment(lib,"urlmon.lib")
std::string tm_to_readable_time(tm ctx);
static std::time_t string_to_timet(std::string timestamp);
static std::tm timet_to_tm(time_t timestamp);
const std::string compilation_date = (std::string)skCrypt(__DATE__);
const std::string compilation_time = (std::string)skCrypt(__TIME__);
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "version.lib")
HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
std::string version = skCrypt("1.0").decrypt(); // leave alone unless you've changed version on website


bool progressPrinted = false;

int progress_callback(void* ptr, curl_off_t totalToDownload, curl_off_t nowDownloaded, curl_off_t totalToUpload, curl_off_t nowUploaded) {
	if (totalToDownload != 0) {
		if (!progressPrinted) {
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			SetConsoleTextAttribute(hConsole, 7);
			std::cout << " [";
			SetConsoleTextAttribute(hConsole, 2);
			std::cout << "+";
			SetConsoleTextAttribute(hConsole, 7);
			std::cout << "] Downloading file...\n";
			progressPrinted = true;
		}

		int percent = static_cast<int>((nowDownloaded * 100) / totalToDownload);

		auto elapsed_time = std::chrono::steady_clock::now() - *static_cast<std::chrono::steady_clock::time_point*>(ptr);
		auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed_time).count();
		auto speed = nowDownloaded / (elapsed_seconds ? elapsed_seconds : 1); // bytes per second
		auto remaining_time = (totalToDownload - nowDownloaded) / (speed ? speed : 1); // seconds
		auto remaining_minutes = remaining_time / 60; // Tính số phút còn lại
		auto remaining_seconds = remaining_time % 60; // Tính số giây còn lại

		std::stringstream ss;
		ss << std::fixed << std::setprecision(2)
			<< (nowDownloaded / 1024.0 / 1024.0) << "/" << (totalToDownload / 1024.0 / 1024.0) << " MB";

		if (remaining_minutes >= 60) {
			auto remaining_hours = remaining_minutes / 60;
			remaining_minutes %= 60;
			ss << " - " << remaining_hours << "h " << remaining_minutes << "m " << remaining_seconds << "s left";
		}
		else if (remaining_minutes > 0) {
			ss << " - " << remaining_minutes << "m " << remaining_seconds << "s left";
		}
		else {
			ss << " - " << remaining_seconds << "s left";
		}

		std::cout << "\r" << std::string(100, ' ') << "\r";
		std::cout << " [";
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 0x0A);
		std::cout << std::string(percent / 2, '=');
		std::cout << std::string(50 - percent / 2, ' ');
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 0x0F);
		std::cout << "]";
		std::cout << percent << "% " << ss.str() << std::flush;
	}
	return 0;
}


// Hàm kiểm tra sự tồn tại của thư mục
bool DirectoryExists(const std::string& dirPath) {
	DWORD fileAttr = GetFileAttributesA(dirPath.c_str());
	return (fileAttr != INVALID_FILE_ATTRIBUTES && (fileAttr & FILE_ATTRIBUTE_DIRECTORY));
}

// Hàm tạo từng thư mục một trong đường dẫn
void CreateDirectoryIfNotExists(const std::string& dirPath) {
	std::string path = "";
	std::vector<std::string> tokens;
	size_t pos = 0, found;

	// Tách đường dẫn theo dấu '\'
	while ((found = dirPath.find_first_of("\\/", pos)) != std::string::npos) {
		tokens.push_back(dirPath.substr(pos, found - pos));
		pos = found + 1;
	}
	tokens.push_back(dirPath.substr(pos));

	// Lần lượt tạo từng phần của đường dẫn
	for (const auto& part : tokens) {
		if (part.empty()) continue;
		path += part + "\\";  // Cộng dồn từng phần vào đường dẫn

		// Kiểm tra và tạo thư mục nếu chưa tồn tại
		if (!DirectoryExists(path)) {
			if (!CreateDirectoryA(path.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
				std::cerr << "Failed to create directory: " << path
					<< " Error code: " << GetLastError() << std::endl;
				exit(1);
			}
		}
	}
}

// Hàm tải file với hiển thị tiến trình
bool DownloadFileWithProgress(const char* url, const char* filePath) {
	CURL* curl;
	FILE* fp;
	CURLcode res;
	curl = curl_easy_init();
	if (curl) {
		fp = fopen(filePath, "wb");
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

		auto start_time = std::chrono::steady_clock::now();
		curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &start_time);

		res = curl_easy_perform(curl);
		fclose(fp);

		if (res != CURLE_OK) {
			std::cerr << "Failed to download file: " << curl_easy_strerror(res) << std::endl;
			curl_easy_cleanup(curl);
			return false;
		}
		curl_easy_cleanup(curl);
		std::cout << std::endl; // Move to the next line after the progress
		return true;
	}
	return false;
}

void PrintDownloadPath(const char* filePath) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, 7); // Màu trắng
	std::cout << " [";
	SetConsoleTextAttribute(hConsole, 2); // Màu xanh lá
	std::cout << "+";
	SetConsoleTextAttribute(hConsole, 7); // Màu trắng
	std::cout << "] Location: " << filePath;
}

// Hàm kiểm tra sự tồn tại của file
bool FileExists(LPCTSTR filePath) {
	return PathFileExists(filePath) == TRUE;
}

// Thực hiện lệnh ADB
void runADBCommand(const std::string& adbPath, const std::string& command) {
	std::cout << "\n ";
	std::string adbCommand = "\"" + adbPath + "\" " + command;
	std::system(adbCommand.c_str());
}


// Lấy danh sách các thiết bị kết nối
std::string getConnectedDevice(const std::string& adbPath) {
	char buffer[128];
	std::string result = "";
	std::string adbCommand = "\"" + adbPath + "\" devices";

	FILE* pipe = _popen(adbCommand.c_str(), "r");
	if (!pipe) throw std::runtime_error("popen() failed!");
	while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
		result += buffer;
	}

	_pclose(pipe);

	size_t pos = result.find("127.0.0.1:5555");
	if (pos != std::string::npos) {
		return "127.0.0.1:5555";
	}
	else {
		pos = result.find("127.0.0.1:5556");
		if (pos != std::string::npos) {
			return "127.0.0.1:5556";
		}
	}

	throw std::runtime_error("No suitable device found.");
}

std::string getFileVersion(const char* filePath) {
	DWORD verHandle = 0;
	DWORD verSize = GetFileVersionInfoSize(filePath, &verHandle);
	if (verSize == 0) {
		return "Unknown version";
	}

	std::vector<char> verData(verSize);
	if (!GetFileVersionInfo(filePath, verHandle, verSize, verData.data())) {
		return "Unknown version";
	}

	VS_FIXEDFILEINFO* fileInfo;
	UINT size = 0;
	if (!VerQueryValue(verData.data(), "\\", (void**)&fileInfo, &size)) {
		return "Unknown version";
	}

	if (size) {
		DWORD versionMS = fileInfo->dwFileVersionMS;
		DWORD versionLS = fileInfo->dwFileVersionLS;

		DWORD major = HIWORD(versionMS);
		DWORD minor = LOWORD(versionMS);
		DWORD build = HIWORD(versionLS);
		DWORD revision = LOWORD(versionLS);

		char versionString[100];
		sprintf_s(versionString, "%d.%d.%d.%d", major, minor, build, revision);
		return std::string(versionString);
	}
	return "Unknown version";
}

bool isProcessRunning(const char* processPath, std::string& version) {
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;
	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hProcessSnap == INVALID_HANDLE_VALUE) {
		return false;
	}

	pe32.dwSize = sizeof(PROCESSENTRY32);
	if (!Process32First(hProcessSnap, &pe32)) {
		CloseHandle(hProcessSnap);
		return false;
	}

	do {
		if (!strcmp(pe32.szExeFile, "HD-Player.exe")) {
			HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
			if (hProcess != NULL) {
				char processFilePath[MAX_PATH];
				if (GetModuleFileNameExA(hProcess, NULL, processFilePath, MAX_PATH)) {
					if (!strcmp(processFilePath, processPath)) {
						version = getFileVersion(processFilePath);
						CloseHandle(hProcess);
						CloseHandle(hProcessSnap);
						return true;
					}
				}
				CloseHandle(hProcess);
			}
		}
	} while (Process32Next(hProcessSnap, &pe32));

	CloseHandle(hProcessSnap);
	return false;
}

bool isBluestacksRunning(std::string& version) {
	const char* isBluestacksPath = "C:\\Program Files\\BlueStacks\\HD-Player.exe";
	return isProcessRunning(isBluestacksPath, version);
}

bool isMSIAppPlayerRunning(std::string& version) {
	const char* msiAppPlayerPath = "C:\\Program Files\\BlueStacks_msi2\\HD-Player.exe";
	return isProcessRunning(msiAppPlayerPath, version);
}

std::atomic<bool> spinnerRunning(true);

void ShowLoadingSpinner() {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	std::string spinner = "|/-\\|/-\\";
	const int SPINNER_COLOR = 2; // Màu xanh lá
	const int DEFAULT_COLOR = 7; // Màu trắng

	size_t i = 0;
	while (spinnerRunning) {
		// Đặt màu cho dấu ngoặc vuông
		SetConsoleTextAttribute(hConsole, DEFAULT_COLOR);
		std::cout << "\r [";

		// Đặt màu cho ký tự spinner
		SetConsoleTextAttribute(hConsole, SPINNER_COLOR);
		std::cout << spinner[i];

		// Đặt lại màu cho dấu ngoặc vuông
		SetConsoleTextAttribute(hConsole, DEFAULT_COLOR);
		std::cout << "] " << std::flush;

		i = (i + 1) % spinner.size(); // Chuyển động qua các ký tự spinner
		Sleep(100); // Chờ 100 mili giây
	}
	std::cout << "\r    \r" << std::flush; // Xóa spinner
}

void Plus()
{
	SetConsoleTextAttribute(hConsole, 7); // Màu trắng
	std::cout << " [";
	SetConsoleTextAttribute(hConsole, 2); // Màu xanh lá
	std::cout << "+";
	SetConsoleTextAttribute(hConsole, 7); // Màu trắng
	std::cout << "] ";
}


void RunExecutable(const char* exePath) {
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	if (CreateProcess(NULL, const_cast<LPSTR>(exePath), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
		// Chờ cho quá trình hoàn thành
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	else {
		std::cerr << "Failed to start process: " << GetLastError() << std::endl;
	}
}

void extractRAR(const std::string& rarFilePath, const std::string& extractToPath) {
	RAROpenArchiveData archiveData;
	memset(&archiveData, 0, sizeof(archiveData));
	archiveData.ArcName = const_cast<char*>(rarFilePath.c_str());
	archiveData.OpenMode = RAR_OM_EXTRACT;

	HANDLE hArc = RAROpenArchive(&archiveData);

	if (archiveData.OpenResult != 0) {
		std::cerr << "Cannot open RAR file: " << archiveData.ArcName << std::endl;
		return;
	}

	RARHeaderData headerData;
	int result;
	while ((result = RARReadHeader(hArc, &headerData)) == 0) {
		if (RARProcessFile(hArc, RAR_EXTRACT, const_cast<char*>(extractToPath.c_str()), nullptr) != 0) {
			Plus();
			std::cerr << "Error extracting file: " << headerData.FileName << std::endl;
			RARCloseArchive(hArc);
			return;
		}
	}

	if (result != ERAR_END_ARCHIVE) {
		Plus();
		std::cerr << "Error reading RAR file: " << result << std::endl;
	}

	RARCloseArchive(hArc);
	std::cout << "\n";
	Plus();
	std::cout << "Extraction successful." << std::endl;

	// Xóa file RAR gốc
	if (std::filesystem::remove(rarFilePath)) {
		Plus();
		std::cout << "Original RAR file deleted successfully.\n";
	}
}


void DownloadAndExtractRar(const std::string& rarUrl, const std::string& rarFilePath, const std::string& extractToPath) {
	// Kiểm tra sự tồn tại của file
	if (std::filesystem::exists(extractToPath)) {
		Plus();
		std::cout << "File already exists: " << extractToPath << std::endl;
		return; // Nếu file đã tồn tại, kết thúc hàm
	}

	// Tải file RAR
	if (DownloadFileWithProgress(rarUrl.c_str(), rarFilePath.c_str())) {
		Plus();
		std::cout << "File downloaded successfully.";

		// Giải nén file RAR
		extractRAR(rarFilePath, extractToPath);
	}
	else {
		Plus();
		std::cerr << "Failed to download RAR file.\n";
	}
}

bool SetRegistryValue(const std::string& subKey, const std::string& valueName, DWORD data) {
	HKEY hKey;
	LONG result = RegCreateKeyExA(
		HKEY_LOCAL_MACHINE,
		subKey.c_str(),
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_SET_VALUE,
		NULL,
		&hKey,
		NULL
	);

	if (result != ERROR_SUCCESS) {
		std::cerr << "Failed to open or create registry key: " << subKey << std::endl;
		return false;
	}

	result = RegSetValueExA(
		hKey,
		valueName.c_str(),
		0,
		REG_DWORD,
		reinterpret_cast<const BYTE*>(&data),
		sizeof(data)
	);

	RegCloseKey(hKey);

	if (result != ERROR_SUCCESS) {
		std::cerr << "Failed to set registry value: " << valueName << std::endl;
		return false;
	}

	return true;
}

// Callback function to write downloaded data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

// Function to get the path of the current executable
std::string getCurrentExecutablePath() {
	char path[MAX_PATH];
	GetModuleFileName(NULL, path, MAX_PATH);
	return std::string(path);
}

// Function to check the version from GitHub
std::string checkVersion() {
	CURL* curl;
	CURLcode res;
	std::string readBuffer;
	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, "https://github.com/duyvanmaster/AIO-LegitVN/blob/master/x64/Release/version.txt");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
	}
	return readBuffer; // Return the latest version from the server
}

// Function to compare versions
bool isNewVersionAvailable(const std::string& currentVersion, const std::string& latestVersion) {
	return currentVersion < latestVersion; // Compare versions
}

// Hàm để thay thế phiên bản cũ bằng phiên bản mới
bool updateSoftware() {
	std::string currentExecutablePath = getCurrentExecutablePath();
	fs::path currentDir = fs::path(currentExecutablePath).parent_path();
	std::string executableName = fs::path(currentExecutablePath).filename().string();
	std::string tempUpdatePath = (currentDir / "update_temp.exe").string();
	std::string updateBatchPath = (currentDir / "update.bat").string();

	std::cout << " Current executable path: " << currentExecutablePath << std::endl;
	std::cout << " Temp update path: " << tempUpdatePath << std::endl;
	std::cout << " Update batch path: " << updateBatchPath << std::endl;

	// Tải file phiên bản mới về vị trí tạm thời
	if (DownloadFileWithProgress("https://raw.githubusercontent.com/duyvanmaster/AIO-LegitVN/master/x64/Release/AIOLegitVN.exe", tempUpdatePath.c_str())) {
		std::cout << " New version downloaded successfully." << std::endl;

		// Tạo file batch để thực hiện cập nhật
		std::ofstream batchFile(updateBatchPath);
		if (!batchFile.is_open()) {
			std::cerr << "Failed to create batch file: " << updateBatchPath << std::endl;
			return false;
		}
		batchFile << "@echo off\n";
		batchFile << "echo Starting update process...\n";
		batchFile << "timeout /t 2 /nobreak >nul\n";
		batchFile << "if exist \"" << tempUpdatePath << "\" (\n";
		batchFile << "    echo Temporary update file exists.\n";
		batchFile << "    move /y \"" << tempUpdatePath << "\" \"" << currentExecutablePath << "\"\n";
		batchFile << "    if errorlevel 1 (\n";
		batchFile << "        echo Update failed. Please try again.\n";
		batchFile << "        pause\n";
		batchFile << "        exit /b 1\n";
		batchFile << "    ) else (\n";
		batchFile << "        echo Update successful.\n";
		batchFile << "        start \"\" \"" << currentExecutablePath << "\"\n";
		batchFile << "    )\n";
		batchFile << ") else (\n";
		batchFile << "    echo Temporary update file not found.\n";
		batchFile << "    pause\n";
		batchFile << "    exit /b 1\n";
		batchFile << ")\n";
		batchFile << "del \"" << updateBatchPath << "\"\n";
		batchFile.close();

		std::cout << "Batch file created successfully." << std::endl;

		// Thực thi file batch và thoát process hiện tại
		SHELLEXECUTEINFO sei = { sizeof(sei) };
		sei.lpVerb = "runas";  // Yêu cầu quyền admin
		sei.lpFile = updateBatchPath.c_str();
		sei.nShow = SW_SHOW;  // Hiển thị cửa sổ cmd để xem tiến trình

		if (ShellExecuteEx(&sei)) {
			std::cout << "Update process started. The application will now close." << std::endl;
			return true;
		}
		else {
			DWORD error = GetLastError();
			std::cerr << "Failed to execute update batch file. Error code: " << error << std::endl;
			return false;
		}
	}
	else {
		std::cerr << "Failed to download the new version." << std::endl;
		return false;
	}
}


void x86() {
	const char* url = "https://dl.cdn.freefiremobile.com/live/package/FreeFire.apk";
	const char* directoryPath = "C:\\LegitVN\\Src";
	const char* filePath = "C:\\LegitVN\\Src\\FFx86LastestVersionLegitVN.apk";
	CreateDirectoryIfNotExists(directoryPath);
	const std::string adbBlueStacksPath = "\"C:\\Program Files\\BlueStacks\\HD-Adb.exe\"";
	const std::string adbMSIPath = "\"C:\\Program Files\\BlueStacks_msi2\\HD-Adb.exe\"";

	if (FileExists(filePath)) {
		Plus();
		std::cout << "File already exists." << std::endl;
		PrintDownloadPath(filePath);
	}
	else {
		if (DownloadFileWithProgress(url, filePath)) {
			Plus();
			std::cout << "APK downloaded successfully." << std::endl;
			PrintDownloadPath(filePath);
		}
		else {
			std::cerr << "Failed to download APK file." << std::endl;
			return; // Exit if the download fails
		}
	}

	auto connectAndInstall = [&](const std::string& adbPath) {
		runADBCommand(adbPath, "connect 127.0.0.1:5555");
		try {
			std::cout << "\n ";
			std::string device = getConnectedDevice(adbPath);
			std::thread spinnerThread(ShowLoadingSpinner);
			// Chạy lệnh cài đặt APK
			runADBCommand(adbPath, "-s " + device + " install " + std::string(filePath));

			// Dừng spinner
			spinnerRunning = false;
			spinnerThread.join(); // Đợi spinner thread kết thúc
			Sleep(10000);
		}
		catch (const std::exception& e) {
			std::cerr << "Error: " << e.what() << std::endl;
		}
		};

	if (isBluestacksRunning(version)) {
		std::cout << "\n";
		Plus();
		std::cout << skCrypt("Bluestacks is running with version: ") << version << std::endl;
		connectAndInstall(adbBlueStacksPath);
	}
	else if (isMSIAppPlayerRunning(version)) {
		Plus();
		std::cout << skCrypt("MSI App Player with version: ") << version << std::endl;
		connectAndInstall(adbMSIPath);
	}
	else {
		SetConsoleTextAttribute(hConsole, 7); // Màu trắng
		std::cout << "\n [";
		SetConsoleTextAttribute(hConsole, 4); // Màu đỏ
		std::cout << "!";
		SetConsoleTextAttribute(hConsole, 7); // Màu trắng
		std::cout << "] ";
		std::cout << skCrypt("Bluestacks and MSI App Player are not running.\n");
		Sleep(3333);
	}
}

void ColorChanger() {
	const char* url = "https://r2.e-z.host/2825fb47-f8a4-472c-9624-df2489f897c0/zviaf378.apk";
	const char* directoryPath = "C:\\LegitVN\\Src";
	const char* filePath = "C:\\LegitVN\\Src\\ColorChangerPro.apk";
	CreateDirectoryIfNotExists(directoryPath);
	const std::string adbBlueStacksPath = "\"C:\\Program Files\\BlueStacks\\HD-Adb.exe\"";
	const std::string adbMSIPath = "\"C:\\Program Files\\BlueStacks_msi2\\HD-Adb.exe\"";

	if (FileExists(filePath)) {
		Plus();
		std::cout << "File already exists." << std::endl;
		PrintDownloadPath(filePath);
	}
	else {
		if (DownloadFileWithProgress(url, filePath)) {
			Plus();
			std::cout << "APK downloaded successfully." << std::endl;
			PrintDownloadPath(filePath);
		}
		else {
			std::cerr << "Failed to download APK file." << std::endl;
			return; // Exit if the download fails
		}
	}

	auto connectAndInstall = [&](const std::string& adbPath) {
		runADBCommand(adbPath, "connect 127.0.0.1:5555");
		try {
			std::cout << "\n ";
			std::string device = getConnectedDevice(adbPath);
			std::thread spinnerThread(ShowLoadingSpinner);
			// Chạy lệnh cài đặt APK
			runADBCommand(adbPath, "-s " + device + " install " + std::string(filePath));

			// Dừng spinner
			spinnerRunning = false;
			spinnerThread.join(); // Đợi spinner thread kết thúc
			Sleep(10000);
		}
		catch (const std::exception& e) {
			std::cerr << "Error: " << e.what() << std::endl;
		}
		};

	if (isBluestacksRunning(version)) {
		std::cout << "\n";
		Plus();
		std::cout << skCrypt("Bluestacks is running with version: ") << version << std::endl;
		connectAndInstall(adbBlueStacksPath);
	}
	else if (isMSIAppPlayerRunning(version)) {
		Plus();
		std::cout << skCrypt("MSI App Player with version: ") << version << std::endl;
		connectAndInstall(adbMSIPath);
	}
	else {
		SetConsoleTextAttribute(hConsole, 7); // Màu trắng
		std::cout << "\n [";
		SetConsoleTextAttribute(hConsole, 4); // Màu đỏ
		std::cout << "!";
		SetConsoleTextAttribute(hConsole, 7); // Màu trắng
		std::cout << "] ";
		std::cout << skCrypt("Bluestacks and MSI App Player are not running.\n");
		Sleep(3333);
	}
}

void InBs4() {
	const char* directoryPath = "C:\\LegitVN\\Src";
	CreateDirectoryIfNotExists(directoryPath);
	const char* Url = "https://r2.e-z.host/2825fb47-f8a4-472c-9624-df2489f897c0/49uhpp4h.exe"; // URL của file rar
	const char* FilePath = "C:\\LegitVN\\Src\\Bluestacks_4.240.20.1016.exe"; // Đường dẫn lưu file rar
	const char* existingFilePath = "C:\\Program Files\\BlueStacks\\Bluestacks.exe"; // Đường dẫn file

	if (FileExists(existingFilePath)) {
		Plus();
		std::cout << "File already exists." << std::endl;
		PrintDownloadPath(existingFilePath);
	}
	else {
		if (FileExists(FilePath)) {

			Plus();
			std::cout << "File already downloaded. Now running the installer...\n";
			PrintDownloadPath(FilePath);
			RunExecutable(FilePath);
		}
		else {
			auto start = std::chrono::steady_clock::now(); // Thời điểm bắt đầu
			if (DownloadFileWithProgress(Url, FilePath)) {
				Plus();
				std::cout << "File downloaded successfully. Now running the installer...\n";
				PrintDownloadPath(FilePath);
				RunExecutable(FilePath);

			}
			else {
				std::cerr << "Failed to download the installer." << std::endl;
			}
		}
	}
}

void InMsi4() {
	const char* Url = "https://r2.e-z.host/2825fb47-f8a4-472c-9624-df2489f897c0/zxpx52s6.exe"; // URL của file rar
	const char* directoryPath = "C:\\LegitVN\\Src";
	const char* FilePath = "C:\\LegitVN\\Src\\MsiPlayer_4.240.15.6305"; // Đường dẫn lưu file rar
	CreateDirectoryIfNotExists(directoryPath);
	const char* existingFilePath = "C:\\Program Files\\BlueStacks_msi2\\Bluestacks.exe"; // Đường dẫn file
	if (FileExists(existingFilePath)) {
		Plus();
		std::cout << "File already exists." << std::endl;
		PrintDownloadPath(existingFilePath);
	}
	else {
		if (FileExists(FilePath)) {
			Plus();
			std::cout << "File already downloaded. Now running the installer...\n";
			PrintDownloadPath(FilePath);
			std::cout << "\n";
			RunExecutable(FilePath);
		}
		else {
			auto start = std::chrono::steady_clock::now(); // Thời điểm bắt đầu
			if (DownloadFileWithProgress(Url, FilePath)) {
				Plus();
				std::cout << "File downloaded successfully. Now running the installer...\n";
				PrintDownloadPath(FilePath);
				RunExecutable(FilePath);

			}
			else {
				std::cerr << "Failed to download the installer." << std::endl;
			}
		}
	}
}

void Root4() {
	const char* directoryPath = "C:\\LegitVN\\Src";
	CreateDirectoryIfNotExists(directoryPath);
	const char* rarUrl = "https://r2.e-z.host/2825fb47-f8a4-472c-9624-df2489f897c0/ab6xqtsp.rar"; // URL của file rar
	const char* rarFilePath = "C:\\LegitVN\\Src\\Root4.rar"; // Đường dẫn lưu file rar
	const char* extractToPath = "C:\\LegitVN\\Src\\Root4"; // Đường dẫn thư mục giải nén
	const char* openfile = "C:\\LegitVN\\Src\\Root4\\BlueStacksTweaker5.exe"; // Đường dẫn thư mục giải nén

	// Gọi hàm DownloadAndExtractRar mà không kiểm tra giá trị trả về
	DownloadAndExtractRar(rarUrl, rarFilePath, extractToPath);
	Plus();
	std::cout << "File downloaded successfully. Now running the installer...\n";
	PrintDownloadPath(openfile);
	RunExecutable(openfile);
}


void Root5() {
	const char* directoryPath = "C:\\LegitVN\\Src";
	CreateDirectoryIfNotExists(directoryPath);
	const char* url = "https://drive.usercontent.google.com/download?id=1aDRSQ-WTc9h10vz3b-4YH8g7Dxkx_jB3&export=download&authuser=4&confirm=t&uuid=878a6d58-6025-41fe-8898-abd02b70282a&at=APZUnTUaPpzkPWAS9FBVT-YshGl5:1721405792257"; // URL của file EXE
	const char* rarUrl = "https://r2.e-z.host/2825fb47-f8a4-472c-9624-df2489f897c0/boce1hz6.rar"; // URL của file rar
	const char* downloadPath = "C:\\LegitVN\\Src\\ZennoDroidEnterpriseDemo-EN-v2.3.9.0.exe"; // Đường dẫn lưu file EXE
	const char* rarFilePath = "C:\\LegitVN\\Src\\Root5.rar"; // Đường dẫn lưu file rar
	const char* existingFilePath = "C:\\Program Files\\ZennoLab\\EN\\ZennoDroidEnterprise Demo\\2.3.9.0\\Progs\\ProjectMakerZD.exe"; // Đường dẫn file đã cài đặt
	const char* extractToPath = "C:\\LegitVN\\Src\\Root5"; // Đường dẫn Thư mục giải nén


	if (FileExists(existingFilePath)) {

		Plus();
		PrintDownloadPath(existingFilePath);
		RunExecutable(existingFilePath);
	}
	else {
		if (FileExists(downloadPath)) {
			Plus();
			std::cout << "File already downloaded. Now running the installer...\n";
			PrintDownloadPath(downloadPath);
			std::cout << "\n";
			RunExecutable(downloadPath);
		}
		else {
			auto start = std::chrono::steady_clock::now(); // Thời điểm bắt đầu
			if (DownloadFileWithProgress(url, downloadPath)) {
				Plus();
				std::cout << "File downloaded successfully. Now running the installer...\n";
				PrintDownloadPath(downloadPath);
				std::cout << "\n";
				DownloadAndExtractRar(rarUrl, rarFilePath, extractToPath);
				PrintDownloadPath(extractToPath);
				RunExecutable(downloadPath);

			}
			else {
				std::cerr << "Failed to download the installer." << std::endl;
			}
		}
	}
}
void FixGPU()
{
	std::string pathBlueStacks = "C:\\Program Files\\BlueStacks";
	std::string pathBlueStacksMsi2 = "C:\\Program Files\\BlueStacks_msi2";

	bool blueStacksSuccess = false;
	bool blueStacksMsi2Success = false;

	// Kiểm tra thư mục BlueStacks và cài đặt Registry
	if (DirectoryExists(pathBlueStacks)) {
		blueStacksSuccess = SetRegistryValue("SOFTWARE\\BlueStacks\\Config", "ForceDedicatedGPU", 1);
	}
	else {
		Plus();
		std::cerr << "Directory does not exist: " << pathBlueStacks << std::endl;
	}

	// Kiểm tra thư mục BlueStacks_msi2 và cài đặt Registry
	if (DirectoryExists(pathBlueStacksMsi2)) {
		blueStacksMsi2Success = SetRegistryValue("SOFTWARE\\BlueStacks_msi2\\Config", "ForceDedicatedGPU", 1);
	}
	else {
		Plus();
		std::cerr << "Directory does not exist: " << pathBlueStacksMsi2 << std::endl;
	}

	// Kiểm tra kết quả và thông báo
	if (blueStacksSuccess && blueStacksMsi2Success) {
		Plus();
		std::cout << "Successfully updated both registry keys." << std::endl;
	}
	else if (blueStacksSuccess) {
		Plus();
		std::cout << "Successfully updated registry key for BlueStacks." << std::endl;
	}
	else if (blueStacksMsi2Success) {
		Plus();
		std::cout << "Successfully updated registry key for BlueStacks_msi2." << std::endl;
	}
	else {
		Plus();
		std::cerr << "Failed to update registry keys." << std::endl;
	}
}

void HighPriority()
{
	std::string paths[] = {
			"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\Bluestacks.exe\\PerfOptions",
			"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\BstkSVC.exe\\PerfOptions",
			"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\HD-Agent.exe\\PerfOptions",
			"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\HD-Player.exe\\PerfOptions"
	};

	bool success = true;

	for (const auto& path : paths) {
		if (!SetRegistryValue(path, "CpuPriorityClass", 3)) {
			success = false;
		}
	}

	if (success) {
		Plus();
		std::cout << "Successfully updated high priority Blustacks & Msi process" << std::endl;
	}
	else {
		Plus();
		std::cerr << "Failed to update some registry keys." << std::endl;
	}
}


void Clumsy() {
	const char* directoryPath = "C:\\LegitVN\\Src";
	CreateDirectoryIfNotExists(directoryPath);
	const char* rarUrl = "https://r2.e-z.host/2825fb47-f8a4-472c-9624-df2489f897c0/gmr97okl.rar"; // URL của file rar
	const char* rarFilePath = "C:\\LegitVN\\Src\\Clumsy.rar"; // Đường dẫn lưu file rar
	const char* extractToPath = "C:\\LegitVN\\Src\\Clumsy"; // Đường dẫn thư mục giải nén
	const char* openfile = "C:\\LegitVN\\Src\\Clumsy\\clumsy.exe"; // Đường dẫn thư mục giải nén

	// Gọi hàm DownloadAndExtractRar mà không kiểm tra giá trị trả về
	DownloadAndExtractRar(rarUrl, rarFilePath, extractToPath);
	Plus();
	std::cout << "File downloaded successfully. Now running the installer...\n";
	PrintDownloadPath(openfile);
	RunExecutable(openfile);
}

void SetProp() {
	std::string adbPath;
	if (isBluestacksRunning(version)) {
		// Đường dẫn đến adb cho BlueStacks
		Plus();
		std::cout << skCrypt("Bluestacks with version: ") << version << std::endl;
		adbPath = "\"C:\\Program Files\\BlueStacks\\HD-Adb.exe\"";
	}
	else if (isMSIAppPlayerRunning(version)) {
		// Đường dẫn đến adb cho MSI App Player
		Plus();
		std::cout << skCrypt("MSI App Player with version: ") << version << std::endl;
		adbPath = "\"C:\\Program Files\\BlueStacks_msi2\\HD-Adb.exe\"";
	}
	else {
		std::cerr << "Both BlueStacks nor MSI App Player is running." << std::endl;
		return;
	}

	std::string command = adbPath + " shell setprop debug.compogition.type mdp";
	system(command.c_str());

	command = adbPath + " shell setprop debug.gr.gapinterval 0";
	system(command.c_str());

	command = adbPath + " shell setprop debug.fb.rgb565 1";
	system(command.c_str());

	command = adbPath + " shell setprop debug.enabletr false"; // Sửa chính tả
	system(command.c_str());

	command = adbPath + " shell setprop debug.gfxemulcompilerfilter speed"; // Sửa chính tả
	system(command.c_str());

	command = adbPath + " shell setprop debug.rg.precision rg_fp_full"; // Sửa chính tả
	system(command.c_str());

	command = adbPath + " shell setprop debug.rg.default-CPU-buffer 262144";
	system(command.c_str());

	command = adbPath + " shell setprop debug.javafx.anim.fullspeed true"; // Rút ngắn tên thuộc tính
	system(command.c_str());

	command = adbPath + " shell setprop debug.javafx.anim.framerate 120"; // Rút ngắn tên thuộc tính
	system(command.c_str());

	command = adbPath + " shell setprop debug.disable.hwacc 0"; // Sửa chính tả
	system(command.c_str());

	command = adbPath + " shell setprop debug.qctwa.preservebuf 1"; // Sửa chính tả
	system(command.c_str());

	command = adbPath + " shell setprop debug.MB.running 72";
	system(command.c_str());

	command = adbPath + " shell setprop debug.MB.inner.running 24";
	system(command.c_str());

	command = adbPath + " shell setprop debug.app.performance_restricte false"; // Sửa chính tả
	system(command.c_str());

	command = adbPath + " shell setprop debug.rg.max-threads 8"; // Sửa chính tả
	system(command.c_str());

	command = adbPath + " shell setprop debug.rg.min-threads 8"; // Sửa chính tả
	system(command.c_str());

	command = adbPath + " shell setprop debug.gr.numframebuffer 0";
	system(command.c_str());

	command = adbPath + " shell setprop debug.power_management_mode perf_max";
	system(command.c_str());

	Plus();
	std::cout << "Performance CPU and GPU settings for the emulator have been successfully." << std::endl;
}

void menu()
{
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt(" [");
	SetConsoleTextAttribute(hConsole, 15);
	std::cout << skCrypt("1");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("]");
	SetConsoleTextAttribute(hConsole, 7);
	std::cout << skCrypt(" Install latest version of FFx86 (Requires adb activation)");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("\n [");
	SetConsoleTextAttribute(hConsole, 15);
	std::cout << skCrypt("2");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("]");
	SetConsoleTextAttribute(hConsole, 7);
	std::cout << skCrypt(" Install Color Changer Pro");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("\n [");
	SetConsoleTextAttribute(hConsole, 15);
	std::cout << skCrypt("3");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("]");
	SetConsoleTextAttribute(hConsole, 7);
	std::cout << skCrypt(" Install Bluestacks 4");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("\n [");
	SetConsoleTextAttribute(hConsole, 15);
	std::cout << skCrypt("4");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("]");
	SetConsoleTextAttribute(hConsole, 7);
	std::cout << skCrypt(" Install Msi Player 4");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("\n [");
	SetConsoleTextAttribute(hConsole, 15);
	std::cout << skCrypt("5");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("]");
	SetConsoleTextAttribute(hConsole, 7);
	std::cout << skCrypt(" Root Bluestacks 4 & Msi Player 4");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("\n [");
	SetConsoleTextAttribute(hConsole, 15);
	std::cout << skCrypt("6");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("]");
	SetConsoleTextAttribute(hConsole, 7);
	std::cout << skCrypt(" Root Bluestacks 5 & Msi Player 5");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("\n [");
	SetConsoleTextAttribute(hConsole, 15);
	std::cout << skCrypt("7");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("]");
	SetConsoleTextAttribute(hConsole, 7);
	std::cout << skCrypt(" Fix GPU Emulators");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("\n [");
	SetConsoleTextAttribute(hConsole, 15);
	std::cout << skCrypt("8");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("]");
	SetConsoleTextAttribute(hConsole, 7);
	std::cout << skCrypt(" High Priority for Bluestacks & Msi process");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("\n [");
	SetConsoleTextAttribute(hConsole, 15);
	std::cout << skCrypt("9");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("]");
	SetConsoleTextAttribute(hConsole, 7);
	std::cout << skCrypt(" Clumsy (Time Stop)");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("\n [");
	SetConsoleTextAttribute(hConsole, 15);
	std::cout << skCrypt("10");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("]");
	SetConsoleTextAttribute(hConsole, 7);
	std::cout << skCrypt(" Setprop Performance CPU & GPU Emualator");

	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("\n\n [");
	SetConsoleTextAttribute(hConsole, 15);
	std::cout << skCrypt(">");
	SetConsoleTextAttribute(hConsole, 8);
	std::cout << skCrypt("]");
	SetConsoleTextAttribute(hConsole, 7);
	std::cout << skCrypt(" Choose option: ");

	SetConsoleTextAttribute(hConsole, 15);
	int option;
	std::cin >> option;
	switch (option)
	{
	case 1:
	{
		system("cls");
		x86();
		Sleep(3000);
		system("cls");
		menu();
		break;
	}
	case 2:
	{
		system("cls");
		ColorChanger();
		Sleep(3000);
		system("cls");
		menu();
		break;
	}
	case 3:
	{
		system("cls");
		InBs4();
		Sleep(3000);
		system("cls");
		menu();
		break;
	}
	case 4:
	{
		system("cls");
		InMsi4();
		Sleep(3000);
		system("cls");
		menu();
		break;
	}
	case 5:
	{
		system("cls");
		Root4();
		Sleep(3000);
		system("cls");
		menu();
		break;
	}
	case 6:
	{
		system("cls");
		Root5();
		Sleep(3000);
		system("cls");
		menu();
		break;
	}
	case 7:
	{
		system("cls");
		FixGPU();
		Sleep(3000);
		system("cls");
		menu();
		break;
	}
	case 8:
	{
		system("cls");
		HighPriority();
		Sleep(3000);
		system("cls");
		menu();
		break;
	}
	case 9:
	{
		system("cls");
		Clumsy();
		Sleep(3000);
		system("cls");
		menu();
		break;
	}
	case 10:
	{
		system("cls");
		SetProp();
		Sleep(3000);
		system("cls");
		menu();
		break;
	}
	default:
		std::cout << "Invalid option. Please choose again.\n";
		break;
	}
}



int main()
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	system("Color 07");
	std::string consoleTitle = (std::string)skCrypt("AIO LegitVN v1.6.3 | Built at:  ") + compilation_date + " " + compilation_time;
	SetConsoleTitleA(consoleTitle.c_str());

	std::string currentVersion = "1.6.3"; // Current version of the application
	std::string latestVersion = checkVersion(); // Get the latest version from the server

	if (isNewVersionAvailable(currentVersion, latestVersion)) {
		std::cout << " New version available: " << latestVersion << ". Proceeding to update." << std::endl;
		updateSoftware();
	}
	else {
		menu();
	}
}


std::string tm_to_readable_time(tm ctx) {
	char buffer[80];

	strftime(buffer, sizeof(buffer), "%a %m/%d/%y %H:%M:%S %Z", &ctx);

	return std::string(buffer);
}

static std::time_t string_to_timet(std::string timestamp) {
	auto cv = strtol(timestamp.c_str(), NULL, 10); // long

	return (time_t)cv;
}

static std::tm timet_to_tm(time_t timestamp) {
	std::tm context;

	localtime_s(&context, &timestamp);

	return context;
	exit(0);
}