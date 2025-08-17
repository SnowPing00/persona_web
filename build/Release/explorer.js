// explorer.js (Stable Version)
$(function () {

    // ========================================================================
    // --- 1. UTILITY FUNCTIONS & GLOBAL STATE ---
    // ========================================================================
    function loadAppScript(app) {
        return new Promise((resolve, reject) => {
            if (!app || !app.entry_point) return reject(new Error("App information is invalid."));
            const scriptId = `app-script-${app.entry_point.replace(/[^a-zA-Z0-9]/g, "-")}`;
            if (document.getElementById(scriptId)) return resolve();
            const script = document.createElement('script');
            script.id = scriptId, script.src = app.entry_point, script.async = true;
            script.onload = () => resolve(), script.onerror = () => reject(new Error(`Failed to load script: ${script.src}`));
            document.head.appendChild(script);
        });
    }


    let installedApps = [];
    let extensionDefaults = {};

    // ========================================================================
    // --- 2. GOLDEN LAYOUT CONFIGURATION & INITIALIZATION ---
    // ========================================================================
    const config = {
        settings: { showPopoutIcon: false, showCloseIcon: false },
        content: [{
            type: 'row',
            content: [
                {
                    type: 'stack', width: 25,
                    content: [
                        { type: 'component', componentName: 'fileBrowser', title: '파일 탐색기', isClosable: false },
                        { type: 'component', componentName: 'appBrowser', title: '앱 목록', isClosable: false }
                    ]
                },
                {
                    type: 'stack',
                    content: [{ type: 'component', componentName: 'fileViewer', title: '뷰어' }]
                }
            ]
        }]
    };
    const myLayout = new GoldenLayout(config);

    // ========================================================================
    // --- 3. COMPONENT REGISTRATION ---
    // ========================================================================
    myLayout.registerComponent('fileBrowser', function (container, componentState) {
        const fileBrowserHtml = `
            <style>
                #file-list li { position: relative; padding: 8px 30px 8px 8px; border-radius: 4px; cursor: pointer; }
                #file-list li:hover { background-color: #333; }
                #file-list li:hover .hamburger-menu { display: block; }
                .hamburger-menu { position: absolute; top: 50%; right: 5px; transform: translateY(-50%); padding: 5px; display: none; }
            </style>
            <ul id="file-list" style="list-style-type: none; padding: 5px; margin: 0; color: white; height: 100%; overflow-y: auto;">
                <li>Loading...</li>
            </ul>`;
        const fileListElement = container.getElement().html(fileBrowserHtml).find('#file-list');

        fileListElement.on('click', 'li', function (event) {
            const fileData = $(this).data('file');
            if (fileData) myLayout.eventHub.emit('file-selected', fileData);
        });

        fileListElement.on('click', '.hamburger-menu', function (event) {
            event.stopPropagation();
            const fileData = $(this).closest('li').data('file');
            if (fileData) {
                const extension = fileData.name.split('.').pop().toLowerCase();
                const compatibleApps = installedApps.filter(app => app.supported_extensions && app.supported_extensions.includes(extension));
                if (compatibleApps.length > 0) {
                    showAppChooser(compatibleApps, fileData);
                } else {
                    alert("이 파일을 열 수 있는 앱이 설치되지 않았습니다.");
                }
            }
        });

        fetch('/api/resources/').then(res => res.json()).then(data => {
            fileListElement.empty();
            data.items.forEach(file => {
                const listItem = $(`<li>${file.isDir ? '📁' : '📄'} ${file.name}<span class="hamburger-menu">☰</span></li>`).data('file', file);
                fileListElement.append(listItem);
            });
        });
    });

    myLayout.registerComponent('appBrowser', function (container, componentState) {
        const appBrowserHtml = `<ul id="app-list" style="list-style-type: none; padding: 5px; margin: 0; color: white; height: 100%; overflow-y: auto;"><li>Loading...</li></ul>`;
        const appListElement = container.getElement().html(appBrowserHtml).find('#app-list');
        fetch('/api/apps').then(res => res.json()).then(apps => {
            appListElement.empty();
            apps.forEach(app => {
                const listItem = $(`<li><strong>${app.name}</strong><div style="font-size: 0.8em; color: #aaa;">${app.description || ''}</div></li>`);
                listItem.on('click', function () {
                    if (app.readme) myLayout.eventHub.emit('app-selected', app);
                });
                appListElement.append(listItem);
            });
        });
    });

    myLayout.registerComponent('fileViewer', function (container, componentState) {
        const viewerElement = container.getElement();
        if (componentState && componentState.appToRun && componentState.fileToOpen) {
            const app = componentState.appToRun;
            const file = componentState.fileToOpen;
            loadAppScript(app).then(() => {
                if (window.personaApps && typeof window.personaApps[app.handler_function] === 'function') {
                    window.personaApps[app.handler_function](file.name, viewerElement);
                } else {
                    viewerElement.html(`<p style="color: red;">Error: Handler '${app.handler_function}' not found.</p>`);
                }
            });
        } else {
            viewerElement.html('<p style="padding: 10px; color: white;">Select a file to view.</p>');
        }
    });

    myLayout.registerComponent('appInfoViewer', function (container, componentState) {
        const viewerElement = container.getElement();
        const app = componentState.app;
        viewerElement.html(`<p style="padding: 10px; color: white;">Loading info for '${app.name}'...</p>`);
        fetch(`/${app.readme}`).then(response => {
            if (!response.ok) throw new Error(`Could not find '${app.readme}' on the server.`);
            return response.text();
        }).then(markdownContent => {
            const htmlContent = marked.parse(markdownContent);
            viewerElement.html(`<div class="markdown-body">${htmlContent}</div>`);
        }).catch(error => {
            viewerElement.html(`<p style="color: red; padding: 10px;">${error.message}</p>`);
        });
    });

    myLayout.registerComponent('infoViewer', function (container, componentState) {
        const message = componentState.message || "Unknown Info";
        const icon = componentState.isError ? '⚠️' : 'ℹ️';
        container.getElement().html(`<div style="padding: 20px; text-align: center; color: white;"><h2 style="font-size: 2em; margin: 0;">${icon}</h2><p>${message}</p></div>`);
    });

    // ========================================================================
    // --- 4. GLOBAL EVENT HANDLERS ---
    // ========================================================================
    myLayout.eventHub.on('file-selected', async function (file) {
        if (file.isDir) return;
        const extension = file.name.split('.').pop().toLowerCase();
        if (extensionDefaults[extension]) {
            const defaultApp = installedApps.find(app => app.name === extensionDefaults[extension]);
            if (defaultApp) {
                executeApp(defaultApp, file);
                return;
            }
        }
        const compatibleApps = installedApps.filter(app => app.supported_extensions && app.supported_extensions.includes(extension));
        if (compatibleApps.length === 1) {
            executeApp(compatibleApps[0], file);
        } else if (compatibleApps.length > 1) {
            showAppChooser(compatibleApps, file);
        } else {
            const viewerStack = myLayout.root.contentItems[0].contentItems[1];
            if (!viewerStack) return;
            const existingInfoViewer = myLayout.root.getItemsByFilter(item => item.componentName === 'infoViewer')[0];
            const message = `'.${extension}' 파일을 열 수 있는 앱이 없습니다.`;
            if (existingInfoViewer) {
                const icon = '⚠️';
                const infoHtml = `<div style="padding: 20px; text-align: center; color: white;"><h2 style="font-size: 2em; margin: 0;">${icon}</h2><p>${message}</p></div>`;
                existingInfoViewer.container.getElement().html(infoHtml);
                existingInfoViewer.parent.setActiveContentItem(existingInfoViewer);
            } else {
                const newItemConfig = { type: 'component', componentName: 'infoViewer', title: '알림', componentState: { message: message, isError: true } };
                viewerStack.addChild(newItemConfig);
            }
        }
    });

    myLayout.eventHub.on('app-selected', async function (app) {
        const openInfoViewers = myLayout.root.getItemsByFilter(item => item.componentName === 'appInfoViewer');
        const existingViewer = openInfoViewers.find(viewer => viewer.container.getState().app.name === app.name);
        if (existingViewer) {
            existingViewer.parent.setActiveContentItem(existingViewer);
        } else {
            const newItemConfig = { type: 'component', componentName: 'appInfoViewer', title: app.name + " 소개", componentState: { app: app } };
            const viewerStack = myLayout.root.contentItems[0].contentItems[1];
            if (viewerStack) viewerStack.addChild(newItemConfig);
        }
    });

    // ========================================================================
    // --- 5. INITIALIZATION & HELPER FUNCTIONS ---
    // ========================================================================
    async function initializeApp() {
        try {
            const response = await fetch('/api/apps');
            installedApps = await response.json();
            myLayout.on('tabCreated', function (tab) {
                if (tab.closeElement) {
                    tab.closeElement.off('click').on('click', (e) => { e.preventDefault(), e.stopPropagation(), showConfirmationDialog(tab); });
                }
            });
            // componentDestroyed logic removed because the core components are no longer closable.
            // Restore button logic is therefore also removed.
            myLayout.init();
        } catch (error) {
            console.error("Application initialization failed:", error);
            // Error logging to server can be added back here if desired.
        }
    }

    function showConfirmationDialog(tab) {
        const targetElement = tab.contentItem.element;
        const viewerStack = myLayout.root.contentItems[0].contentItems[1];
        if (tab.contentItem.parent === viewerStack && viewerStack.contentItems.length === 1) {
            alert("마지막 뷰어 창은 닫을 수 없습니다.");
            return;
        }
        if (targetElement.find('.local-backdrop').length > 0) return;
        const dialog = $('#confirmation-dialog');
        const confirmBtn = $('#confirm-close-btn');
        const cancelBtn = $('#cancel-close-btn');
        const backdrop = $('<div class="local-backdrop"></div>');
        targetElement.find('.lm_content').append(backdrop);
        const rect = targetElement[0].getBoundingClientRect();
        const dialogTop = rect.top + (rect.height / 2) - (dialog.outerHeight() / 2);
        const dialogLeft = rect.left + (rect.width / 2) - (dialog.outerWidth() / 2);
        dialog.css({ top: dialogTop + 'px', left: dialogLeft + 'px' }).show();
        confirmBtn.off('click').one('click', () => { tab.contentItem.remove(), dialog.hide(), backdrop.remove(); });
        cancelBtn.off('click').one('click', () => { dialog.hide(), backdrop.remove(); });
    }

    async function executeApp(app, file) {
        const extension = file.name.split('.').pop().toLowerCase();
        try {
            await loadAppScript(app);
            const openViewers = myLayout.root.getItemsByFilter(item => item.componentName === 'fileViewer');
            const existingViewer = openViewers.find(viewer => {
                const state = viewer.container.getState();
                return state && state.fileToOpen && state.fileToOpen.name === file.name;
            });
            if (existingViewer) {
                existingViewer.parent.setActiveContentItem(existingViewer);
            } else {
                const newItemConfig = { type: 'component', componentName: 'fileViewer', title: file.name, componentState: { appToRun: app, fileToOpen: file } };
                const viewerStack = myLayout.root.contentItems[0].contentItems[1];
                if (viewerStack) viewerStack.addChild(newItemConfig);
            }
            extensionDefaults[extension] = app.name;
        } catch (error) {
            console.error("Error executing app:", error);
        }
    }

    function showAppChooser(apps, file) {
        const dialog = $('#app-chooser-dialog');
        const backdrop = $('#app-chooser-backdrop');
        const appList = $('#app-choice-list');
        const cancelBtn = $('#cancel-app-choice-btn');
        appList.empty();
        apps.forEach(app => {
            const listItem = $(`<li>${app.name}</li>`);
            listItem.on('click', function () {
                executeApp(app, file);
                dialog.hide();
                backdrop.hide();
            });
            appList.append(listItem);
        });
        cancelBtn.off('click').one('click', function () {
            dialog.hide();
            backdrop.hide();
        });
        backdrop.show();
        dialog.show();
    }

    // ========================================================================
    // --- 6. START THE APPLICATION ---
    // ========================================================================
    initializeApp();
});