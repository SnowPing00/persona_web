# Persona Workspace v1.0

A secure, local-first web desktop environment built with C++ and modern web technologies. This project demonstrates a powerful combination of a native C++ backend and a dynamic JavaScript frontend to create a sandboxed workspace within your browser.



## 💡 Core Concepts

* **Local-First Principle**: All user data is stored exclusively on the user's local machine in a dedicated virtual drive (`C:\PersonaRoot`), ensuring complete data sovereignty and privacy.
* **Secure Sandbox**: The application operates within a secure sandbox. The frontend can only interact with files inside the virtual drive through a strictly controlled C++ backend API.
* **Modular App System**: New applications (viewers, editors, etc.) can be easily added by creating a new folder with a `manifest.json` file, without modifying the core application code.
* **Dynamic UI**: The user interface is built on Golden Layout, allowing users to drag, drop, resize, and manage windows to create their own personalized workspace.

---

## ✨ Features

* **Dynamic Multi-Window UI**: A fully customizable desktop environment powered by Golden Layout.
* **Virtual File Explorer**: Browse and interact with files and directories within your secure local virtual drive.
* **App Browser**: Discover and view information about installed applications.
* **Smart File Handling**:
    * **"Open With..." Dialog**: If multiple apps can open a file, a selection dialog is presented.
    * **Persistent Default Apps**: The system remembers your app choice for each file type for quick access.
    * **Context Menu**: A hamburger menu on each file allows you to explicitly choose which app to open it with.
* **Secure C++ Backend**: A multi-threaded web server handles all file operations, app discovery, and serves the frontend.
* **Safety Features**: Includes close confirmation dialogs and prevents closing essential windows to avoid accidental data loss.

---

## 🔧 Prerequisites & Setup

Before running the application, you **must** create a specific folder on your `C:` drive. This folder will act as the root of the Persona Virtual Drive.

1.  **Open Command Prompt or PowerShell as an Administrator.**
2.  **Create the directory by running the following command:**

    ```bash
    mkdir C:\PersonaRoot
    ```

This directory is required for all file-related operations. Without it, the server-side file APIs will fail.

---

## 🛠️ Built With

This project was made possible by several amazing open-source libraries:

* **Backend (C++)**:
    * [WinFsp](https://github.com/winfsp/winfsp) - For creating the user-mode virtual file system. (Licensed under GPLv3 with a favorable exception clause).
    * [cpp-httplib](https://github.com/yhirose/cpp-httplib) - A header-only, cross-platform HTTP/HTTPS library. (MIT License)
    * [JSON for Modern C++](https://github.com/nlohmann/json) - A seamless JSON library for C++. (MIT License)
* **Frontend (JavaScript)**:
    * [Golden Layout](https://golden-layout.com/) - A powerful multi-window layout manager for web applications. (MIT License)
    * [jQuery](https://jquery.com/) - A fast, small, and feature-rich JavaScript library. (MIT License)
    * [Marked.js](https://marked.js.org/) - A low-level compiler for parsing Markdown. (MIT License)

---

## 📜 License

This project is licensed under the **MIT License**. See the `LICENSE` file for details.
