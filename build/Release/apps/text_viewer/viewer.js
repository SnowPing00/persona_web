window.personaApps = window.personaApps || {};

window.personaApps.renderTextViewer = function(filename, container) {
    container.html(`<p style="padding: 10px; color: white;">'${filename}' 내용 로딩 중...</p>`);

    fetch(`/api/readfile?filename=${encodeURIComponent(filename)}`)
        .then(response => response.text())
        .then(content => {
            const safeContent = $('<textarea />').text(content).html();
            container.html(`
                <h3 style="padding: 0 10px; color: white;">${filename}</h3>
                <textarea readonly style="width:100%; height: 90%; box-sizing: border-box; background-color: #1e1e1e; color: #d4d4d4; border: none;">${safeContent}</textarea>
            `);
        });
};