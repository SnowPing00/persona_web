#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#include "httplib.h"
#include <iostream>
#include <string>
#include <vector>
#include <locale>
#include <codecvt>
#include <fstream>
#include <windows.h>
#include <stdexcept>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include "nlohmann/json.hpp"

/**
 * @brief Declares the function to start the virtual filesystem.
 *
 * This function is defined in another source file (likely related to the Dokan/WinFsp library).
 * 'extern "C"' ensures C-style linkage, allowing this function to be called from other
 * parts of the application (or other languages) without C++ name mangling issues.
 */
extern "C" int start_filesystem(int argc, wchar_t* argv[]);


/**
 * @brief The main function for the web server thread.
 *
 * This function is designed to be executed in a separate thread via the Windows API's CreateThread.
 * It takes a pointer to an httplib::Server object and starts its listening loop.
 * Running the server in its own thread is crucial because svr->listen() is a blocking call
 * that would otherwise freeze the main application's UI or other operations.
 *
 * @param lpParam A void pointer which is expected to be a pointer to the httplib::Server instance.
 * @return DWORD The thread's exit code (0 indicates success).
 */
DWORD WINAPI run_server(LPVOID lpParam) {
    // Cast the void pointer argument back to a usable httplib::Server pointer.
    httplib::Server* svr = (httplib::Server*)lpParam;

    // Print a startup message to the console for debugging purposes.
    std::cout << "Server has started on http://localhost:1234" << std::endl;

    // Start the server's blocking listening loop. This function will continuously wait for
    // and handle incoming HTTP requests until the server is stopped.
    svr->listen("localhost", 1234);

    // Standard return value for a successfully terminated thread.
    return 0;
}

/**
 * @brief Converts a wide string (UTF-16 on Windows) to a UTF-8 encoded string.
 *
 * This function is necessary for handling file paths and other strings that may
 * contain non-ASCII characters, ensuring they are compatible with web standards
 * and libraries that expect UTF-8. It uses the Windows API's WideCharToMultiByte.
 *
 * @param wstr The wide string (std::wstring) to convert.
 * @return std::string The resulting UTF-8 encoded string.
 */
std::string wstring_to_utf8(const std::wstring& wstr)
{
    // If the input string is empty, return an empty string immediately to avoid errors.
    if (wstr.empty()) return std::string();

    // Step 1: Calculate the required buffer size for the output UTF-8 string.
    // We call WideCharToMultiByte with a NULL output buffer to ask it how much space is needed.
    int size_needed = WideCharToMultiByte(
        CP_UTF8,      // The target character set (UTF-8).
        0,            // Default flags.
        &wstr[0],     // Pointer to the start of the wide string.
        (int)wstr.size(), // Length of the wide string.
        NULL,         // NULL for the output buffer to calculate size.
        0,            // 0 for the output buffer size.
        NULL,         // Not used.
        NULL          // Not used.
    );

    // Step 2: Perform the actual conversion.
    // Create a string of the calculated size, initialized with zeros.
    std::string strTo(size_needed, 0);

    // Call the function again, this time providing the output buffer.
    WideCharToMultiByte(
        CP_UTF8,
        0,
        &wstr[0],
        (int)wstr.size(),
        &strTo[0],    // Pointer to the start of the output string buffer.
        size_needed,  // The size of the output buffer.
        NULL,
        NULL
    );

    // Return the converted UTF-8 string.
    return strTo;
}

/**
 * @brief Converts a UTF-8 encoded string to a wide string (UTF-16 on Windows).
 *
 * This is the reverse of the wstring_to_utf8 function. It's useful for when you need
 * to pass a standard string to a Windows API function that expects a wide string
 * (LPCWSTR), especially for handling file paths with international characters.
 *
 * @param str The UTF-8 encoded string (std::string) to convert.
 * @return std::wstring The resulting wide string (UTF-16).
 */
std::wstring utf8_to_wstring(const std::string& str)
{
    // Return immediately if the input string is empty.
    if (str.empty()) return std::wstring();

    // Step 1: Calculate the required buffer size for the output wide string.
    // Call MultiByteToWideChar with a NULL output buffer to get the needed size.
    int size_needed = MultiByteToWideChar(
        CP_UTF8,      // The source character set (UTF-8).
        0,            // Default flags.
        &str[0],      // Pointer to the start of the source string.
        (int)str.size(), // Length of the source string in bytes.
        NULL,         // NULL for the output buffer to calculate size.
        0             // 0 for the output buffer size.
    );

    // Step 2: Perform the actual conversion.
    // Create a wide string of the calculated size.
    std::wstring wstrTo(size_needed, 0);

    // Call the function again, this time providing the destination buffer.
    MultiByteToWideChar(
        CP_UTF8,
        0,
        &str[0],
        (int)str.size(),
        &wstrTo[0],   // Pointer to the start of the destination buffer.
        size_needed   // The size of the destination buffer.
    );

    // Return the converted wide string.
    return wstrTo;
}

/**
 * @brief Acts as a security checkpoint to prevent directory traversal attacks.
 *
 * This function takes a filename provided by a user, combines it with the virtual
 * drive's root path (C:\PersonaRoot\), and verifies that the final, fully-resolved
 * path is still safely within that root directory. This is crucial for preventing
 * users from accessing unauthorized files using relative paths like "../../Windows/System32/".
 *
 * @param requested_filename The filename or relative path from the user's request.
 * @param out_full_path An output parameter that will be filled with the safe,
 * canonical path if the check is successful.
 * @return true if the path is safe and within C:\PersonaRoot\, false otherwise.
 */
bool is_safe_path(const std::wstring& requested_filename, std::wstring& out_full_path)
{
    // 1. Combine the base path of the virtual drive with the requested filename.
    std::wstring combined_path = L"C:\\PersonaRoot\\" + requested_filename;

    // 2. Use the Windows API to resolve the path into its canonical, absolute form.
    // This function is key as it processes any potentially malicious ".." or "." components.
    wchar_t final_path_buffer[MAX_PATH];
    if (GetFullPathNameW(combined_path.c_str(), MAX_PATH, final_path_buffer, NULL) == 0) {
        // If the path calculation fails (e.g., due to invalid characters), consider it unsafe.
        return false;
    }

    // 3. Final defense: Check if the fully resolved path starts with our safe directory prefix.
    std::wstring safe_prefix = L"C:\\PersonaRoot\\";
    if (wcsncmp(final_path_buffer, safe_prefix.c_str(), safe_prefix.length()) != 0) {
        // If the resolved path has "escaped" the safe directory, treat it as a hostile request and block it.
        return false;
    }

    // 4. If all checks pass, provide the safe, canonical path via the output parameter and report success.
    out_full_path = final_path_buffer;
    return true;
}

/**
 * @brief Determines the MIME type of a file based on its extension.
 *
 * This helper function is used to set the correct `Content-Type` HTTP header
 * when serving static files. This tells the browser how to properly interpret
 * the content (e.g., as an HTML document, a JavaScript file, a CSS stylesheet, etc.).
 *
 * @param path The file path or filename to check.
 * @return std::string The corresponding MIME type as a string.
 */
std::string get_mime_type(const std::string& path) {
    // Find the last occurrence of the extension substring to identify the file type.
    // Note: A more robust implementation might first isolate the extension after the last '.'.
    if (path.rfind(".html") != std::string::npos) return "text/html";
    if (path.rfind(".js") != std::string::npos) return "application/javascript";
    if (path.rfind(".css") != std::string::npos) return "text/css";
    // ... Other common types like .json, .png, .jpg could be added here ...

    // If the file type is unknown, return the generic binary stream type.
    // This typically instructs the browser to download the file rather than trying to display it.
    return "application/octet-stream";
}

/**
 * @brief Initializes and starts the web server.
 *
 * This function is the main entry point for all web server functionality.
 * It creates a persistent server instance, sets up all API routes (handlers),
 * and launches the server's listening loop in a separate thread.
 * 'extern "C"' allows this function to be called from the main application.
 *
 * @return int Returns 0 upon successful initialization.
 */
extern "C" int start_web_server() {
    // Declare the server object as 'static'.
    // This ensures that the server is created only once, the first time this function is called,
    // and persists for the entire lifetime of the application. This is crucial for maintaining
    // a single, consistent server instance across potential multiple calls.
    static httplib::Server server;

    /**
 * @brief Handles GET requests to list resources (files/directories) in the virtual drive.
 *
 * This API endpoint is designed for the file browser UI. It accepts a path and returns
 * a JSON object containing information about that path and, if it's a directory,
 * a list of its contents.
 *
 * The route uses a regular expression R"(/api/resources(/.*)?)" to capture the
 * optional path that follows "/api/resources/". For example, "/api/resources/MyFolder".
 */
    server.Get(R"(/api/resources(/.*)?)", [&](const httplib::Request& req, httplib::Response& res) {
        // Set a CORS header to allow requests from any web origin.
        res.set_header("Access-Control-Allow-Origin", "*");

        try {
            // --- 1. Process the requested path from the URL ---
            // req.matches[1] contains the part of the URL captured by the regex's parentheses (e.g., "/MyFolder").
            std::string requested_path_utf8 = req.matches[1].str();

            // Sanitize the path by removing the leading slash for consistency.
            if (!requested_path_utf8.empty() && requested_path_utf8[0] == '/') {
                requested_path_utf8 = requested_path_utf8.substr(1);
            }
            // Convert the UTF-8 path to a wide string for Windows API compatibility.
            std::wstring requested_path_wide = utf8_to_wstring(requested_path_utf8);

            // --- 2. Perform security check ---
            // Pass the requested path through our security checkpoint.
            std::wstring full_path;
            if (!is_safe_path(requested_path_wide, full_path)) {
                // If the path is outside the safe "C:\PersonaRoot\" directory, deny access.
                res.status = 403; // 403 Forbidden
                res.set_content("Forbidden", "text/plain");
                return;
            }

            // --- 3. Build the JSON response ---
            nlohmann::json response_json;
            // The file browser UI expects a specific JSON structure.
            response_json["name"] = wstring_to_utf8(std::filesystem::path(full_path).filename().wstring());
            response_json["isDir"] = std::filesystem::is_directory(full_path);
            response_json["items"] = nlohmann::json::array(); // Initialize an empty array for directory contents.
            response_json["path"] = "/" + requested_path_utf8;

            // If the requested path points to a directory, list its contents.
            if (response_json["isDir"]) {
                for (const auto& entry : std::filesystem::directory_iterator(full_path)) {
                    nlohmann::json item;
                    item["name"] = wstring_to_utf8(entry.path().filename().wstring());
                    item["isDir"] = entry.is_directory();
                    // Note: More properties like size and modification date could be added here.

                    response_json["items"].push_back(item);
                }
            }

            // --- 4. Send the response ---
            // Convert the JSON object to a string and send it back to the client.
            res.set_content(response_json.dump(), "application/json; charset=utf-8");

        }
        catch (const std::exception& e) {
            // If any unexpected error occurs, send a 500 Internal Server Error response.
            res.status = 500;
            res.set_content(e.what(), "text/plain");
        }
        });

    /**
 * @brief Handles GET requests to read the content of a specific file.
 *
 * This API endpoint takes a 'filename' query parameter and returns the raw
 * content of that file from the virtual drive (C:\PersonaRoot\).
 * It performs a critical security check to ensure the requested file is
 * safely within the virtual drive's boundaries.
 */
    server.Get("/api/readfile", [](const httplib::Request& req, httplib::Response& res) {
        // Set a CORS header to allow requests from any web origin.
        res.set_header("Access-Control-Allow-Origin", "*");

        // Check if the required 'filename' query parameter exists in the URL.
        if (req.has_param("filename")) {
            // --- 1. Get and process the filename ---
            // Get the filename from the query parameter (e.g., /api/readfile?filename=MyFile.txt).
            std::string utf8_filename = req.get_param_value("filename");
            // Convert to a wide string for Windows API compatibility.
            std::wstring wide_filename = utf8_to_wstring(utf8_filename);

            // --- 2. Perform security check ---
            std::wstring safe_full_path;
            // Pass the requested filename through our security checkpoint.
            if (!is_safe_path(wide_filename, safe_full_path)) {
                // If the security check fails, deny access with a 403 Forbidden error.
                res.status = 403;
                res.set_content("Forbidden: Path is not safe.", "text/plain");
                return; // Stop execution immediately.
            }

            // --- 3. Read and return the file content ---
            // Open the file from the now-verified safe path in binary mode.
            std::ifstream infile(safe_full_path, std::ios::binary);
            if (infile.is_open()) {
                // Read the entire file content into a string in one go.
                std::string content((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
                infile.close();

                // Send the file content as the response.
                res.set_content(content, "text/plain; charset=utf-8");
            }
            else {
                // If the file could not be opened (e.g., it doesn't exist), return a 404 Not Found error.
                res.status = 404;
                res.set_content("File not found or could not be opened.", "text/plain");
            }
        }
        else {
            // If the 'filename' parameter was not provided, return a 400 Bad Request error.
            res.status = 400;
            res.set_content("Filename parameter is missing.", "text/plain");
        }
        });

    /**
 * @brief Handles GET requests to stream large file content (e.g., video, audio).
 *
 * This endpoint is optimized for large files. Instead of reading the whole file into
 * memory, it uses a content provider to send the file in smaller chunks. This is
 * essential for features like video seeking (HTTP Range requests) and for handling
 * files that are too large to fit in RAM.
 */
    server.Get("/api/streamfile", [](const httplib::Request& req, httplib::Response& res) {
        // Set a CORS header to allow requests from any web origin.
        res.set_header("Access-Control-Allow-Origin", "*");

        // --- 1. Validate request and path ---
        // Ensure the required 'filename' query parameter was provided.
        if (!req.has_param("filename")) {
            res.status = 400; // Bad Request
            res.set_content("Filename parameter is missing.", "text/plain");
            return;
        }

        std::string utf8_filename = req.get_param_value("filename");
        std::wstring wide_filename = utf8_to_wstring(utf8_filename);

        // Perform the same security check as /api/readfile to prevent directory traversal.
        std::wstring safe_full_path;
        if (!is_safe_path(wide_filename, safe_full_path)) {
            res.status = 403; // Forbidden
            res.set_content("Forbidden: Path is not safe.", "text/plain");
            return;
        }

        // --- 2. Open the file for streaming ---
        // Use a shared_ptr to manage the file stream's lifetime within the lambda captures.
        auto file_stream = std::make_shared<std::ifstream>(safe_full_path, std::ios::binary);

        // If the file cannot be opened, return a 404 Not Found error.
        if (!file_stream->is_open()) {
            res.status = 404;
            res.set_content("File not found.", "text/plain");
            return;
        }

        // --- 3. Get file size ---
        // Seek to the end of the file to determine its total size.
        file_stream->seekg(0, std::ios::end);
        auto file_size = file_stream->tellg();
        // Seek back to the beginning to prepare for reading.
        file_stream->seekg(0, std::ios::beg);

        // --- 4. Set up the content provider for streaming ---
        // This tells httplib how to fetch chunks of the file on demand.
        res.set_content_provider(
            file_size,      // 1. The total size of the content.
            "video/mp4",    // 2. The MIME type. Note: This should be determined dynamically.

            // 3. This lambda function is the core of the streaming.
            //    httplib will call it whenever it needs another chunk of data.
            [file_stream](size_t offset, size_t length, httplib::DataSink& sink) {
                // Move the read position in the file to the requested offset.
                file_stream->seekg(offset);

                // Create a buffer and read the requested 'length' of bytes from the file.
                std::vector<char> buffer(length);
                file_stream->read(buffer.data(), length);

                // Send the actual number of bytes read (gcount) to the client.
                sink.write(buffer.data(), file_stream->gcount());

                return true; // Return true to continue streaming.
            },

            // 4. This lambda is called after the entire file has been sent.
            [file_stream](bool success) {
                // Clean up by closing the file stream.
                file_stream->close();
            }
        );
        });

    /**
 * @brief Handles GET requests to discover and list all available applications.
 *
 * This endpoint scans the "./apps/" directory on the server, looking for subdirectories
 * that represent individual applications. It reads the "manifest.json" from each one,
 * processes the paths within it to be web-accessible, and returns a JSON array
 * of all valid app configurations to the frontend.
 */
    server.Get("/api/apps", [](const httplib::Request& req, httplib::Response& res) {
        // Set a CORS header to allow requests from any web origin.
        res.set_header("Access-Control-Allow-Origin", "*");

        // --- 1. Scan the "./apps/" directory for subdirectories ---
        // Use the Windows API to find all files and folders in the "apps" directory.
        std::wstring apps_dir_path = L".\\apps\\*";
        WIN32_FIND_DATAW find_data;
        HANDLE find_handle = FindFirstFileW(apps_dir_path.c_str(), &find_data);

        // This will hold the list of all processed app manifests.
        nlohmann::json apps_list = nlohmann::json::array();

        if (find_handle != INVALID_HANDLE_VALUE) {
            // Loop through all items found in the "apps" directory.
            do {
                std::wstring dir_name = find_data.cFileName;

                // Process the item only if it's a directory and not "." or "..".
                if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                    dir_name != L"." && dir_name != L"..")
                {
                    // --- 2. Read and parse the manifest.json for each app ---
                    std::wstring manifest_path = L".\\apps\\" + dir_name + L"\\manifest.json";
                    std::ifstream manifest_file(manifest_path);

                    if (manifest_file.is_open()) {
                        try {
                            nlohmann::json manifest_json;
                            manifest_file >> manifest_json;

                            // --- 3. IMPORTANT: Convert relative paths to web-accessible paths ---
                            // Get the original entry_point (e.g., "viewer.js").
                            std::string original_entry_point = manifest_json["entry_point"];
                            // Prepend the app's directory to create a full path (e.g., "apps/text_viewer/viewer.js").
                            manifest_json["entry_point"] = "apps/" + wstring_to_utf8(dir_name) + "/" + original_entry_point;

                            // Do the same for the "readme" path if it exists.
                            if (manifest_json.contains("readme")) {
                                std::string original_readme = manifest_json["readme"];
                                manifest_json["readme"] = "apps/" + wstring_to_utf8(dir_name) + "/" + original_readme;
                            }

                            // Add the modified manifest object to our list of apps.
                            apps_list.push_back(manifest_json);
                        }
                        catch (const std::exception& e) {
                            // If parsing fails for one manifest, print an error and continue with the next.
                            std::cerr << "JSON parse error in " << wstring_to_utf8(manifest_path) << ": " << e.what() << std::endl;
                        }
                    }
                }
            } while (FindNextFileW(find_handle, &find_data) != 0);
            // Clean up and close the file search handle.
            FindClose(find_handle);
        }

        // For debugging: print the final JSON data to the server console.
        std::cout << "Final JSON data sent to frontend:\n" << apps_list.dump(4) << std::endl;

        // --- 4. Send the final list to the frontend ---
        res.set_content(apps_list.dump(), "application/json; charset=utf-8");
        });

    /**
 * @brief Handles POST requests to create or overwrite a file in the virtual drive.
 *
 * This endpoint expects a JSON body containing a "filename" and "content".
 * It performs a security check on the filename and then writes the provided
 * content to the specified file within the C:\PersonaRoot\ directory.
 * If the file already exists, its content will be replaced.
 */
    server.Post("/api/writefile", [](const httplib::Request& req, httplib::Response& res) {
        // Set a CORS header to allow requests from any web origin.
        res.set_header("Access-Control-Allow-Origin", "*");

        try {
            // --- 1. Parse the incoming JSON request body ---
            // Example expected body: {"filename": "new.txt", "content": "hello world"}
            auto json_body = nlohmann::json::parse(req.body);
            std::string utf8_filename = json_body["filename"];
            std::string content = json_body["content"];

            // Convert the filename to a wide string to properly handle non-ASCII characters on Windows.
            std::wstring wide_filename = utf8_to_wstring(utf8_filename);

            // --- 2. Perform security check ---
            std::wstring safe_full_path;
            // It's critical to validate the path to prevent writing files outside the virtual drive.
            if (!is_safe_path(wide_filename, safe_full_path)) {
                // If the security check fails, throw an error to be caught below.
                throw std::runtime_error("Path is not safe");
            }

            // --- 3. Write the content to the file ---
            // Open the file at the verified safe path. std::ofstream overwrites by default.
            std::ofstream outfile(safe_full_path);
            if (outfile.is_open()) {
                outfile << content;
                outfile.close();
                // Send a success response back to the client.
                res.set_content("{\"status\": \"success\", \"filename\": \"" + utf8_filename + "\"}", "application/json");
            }
            else {
                // If the file could not be opened for writing, throw an error.
                throw std::runtime_error("Failed to open file for writing");
            }
        }
        catch (const std::exception& e) {
            // If any error occurs (JSON parsing, security check, file I/O),
            // send a 500 Internal Server Error response with the error message.
            res.status = 500;
            res.set_content("{\"status\": \"error\", \"message\": \"" + std::string(e.what()) + "\"}", "application/json");
        }
        });

    server.Post("/api/deletefile", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*"); // CORS 허가증

        // 요청 본문(body)에서 삭제할 파일 이름을 JSON 형태로 받습니다.
        // 예: {"filename": "test.txt"}
        try {
            auto json_body = nlohmann::json::parse(req.body);
            std::string utf8_filename = json_body["filename"];

            // 한글 파일 처리를 위해 UTF-8 -> UTF-16 변환
            std::wstring wide_filename = utf8_to_wstring(utf8_filename);
            std::wstring wide_filepath = L"C:\\PersonaRoot\\" + wide_filename;

            // 보안 검사
            if (wide_filename.find(L"..") != std::wstring::npos) {
                throw std::runtime_error("Invalid filename");
            }

            // Windows API를 사용해 파일을 삭제합니다.
            if (DeleteFileW(wide_filepath.c_str())) {
                res.set_content("{\"status\": \"success\"}", "application/json");
            }
            else {
                throw std::runtime_error("Failed to delete file");
            }
        }
        catch (const std::exception& e) {
            res.status = 500; // 서버 내부 오류
            res.set_content("{\"status\": \"error\", \"message\": \"" + std::string(e.what()) + "\"}", "application/json");
        }
        });

    server.Post("/api/updatefile", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*"); // CORS 허가증

        try {
            // 요청 본문을 JSON으로 파싱합니다. 예: {"filename": "test.txt", "content": "hello world"}
            auto json_body = nlohmann::json::parse(req.body);
            std::string utf8_filename = json_body["filename"];
            std::string content = json_body["content"];

            // 한글 파일 처리를 위해 UTF-8 -> UTF-16 변환
            std::wstring wide_filename = utf8_to_wstring(utf8_filename);
            std::wstring wide_filepath = L"C:\\PersonaRoot\\" + wide_filename;

            // 보안 검사
            if (wide_filename.find(L"..") != std::wstring::npos) {
                throw std::runtime_error("Invalid filename");
            }

            // 파일을 쓰기 모드로 열고, 기존 내용을 덮어씁니다.
            std::ofstream outfile(wide_filepath, std::ios::trunc); // std::ios::trunc 플래그가 덮어쓰기 모드입니다.
            if (outfile.is_open()) {
                outfile << content;
                outfile.close();
                res.set_content("{\"status\": \"success\"}", "application/json");
            }
            else {
                throw std::runtime_error("Failed to open file for writing");
            }
        }
        catch (const std::exception& e) {
            res.status = 500; // 서버 내부 오류
            res.set_content("{\"status\": \"error\", \"message\": \"" + std::string(e.what()) + "\"}", "application/json");
        }
        });

    server.Get(R"((/.*))", [&](const httplib::Request& req, httplib::Response& res) {
        std::string path = req.matches[1];
        if (path == "/") path = "/index.html";

        // substr(1)을 사용하여 맨 앞의 '/'를 제거합니다.
        std::filesystem::path file_path = std::filesystem::current_path() / path.substr(1);

        if (std::filesystem::exists(file_path) && std::filesystem::is_regular_file(file_path)) {
            std::ifstream ifs(file_path, std::ios::binary);
            std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            res.set_content(content, get_mime_type(path).c_str());
        }
        else {
            res.status = 404;
            res.set_content("File not found", "text/plain");
        }
        });

    server.Post("/api/log", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // 1. 현재 시간을 가져옵니다.
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm buf;
            localtime_s(&buf, &in_time_t);
            std::stringstream ss;
            ss << std::put_time(&buf, "%Y-%m-%d %X");

            // 2. 로그 파일(persona_error.log)을 추가 모드로 엽니다.
            std::ofstream log_file("persona_error.log", std::ios::app);
            if (log_file.is_open()) {
                // 3. 로그 메시지를 형식에 맞게 작성합니다.
                // 예: [2025-08-17 14:30:00] ERROR: 앱 초기화 실패: ...
                log_file << "[" << ss.str() << "] ERROR: " << req.body << std::endl;
                log_file.close();
            }

            res.set_content("{\"status\": \"logged\"}", "application/json");
        }
        catch (const std::exception& e) {
            res.status = 500;
            res.set_content(e.what(), "text/plain");
        }
        });

    CreateThread(NULL, 0, run_server, &server, 0, NULL);

    std::cout << "Web server is running..." << std::endl;

    return 0;
}