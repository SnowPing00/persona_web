window.personaApps = window.personaApps || {};

window.personaApps.renderMarkdownViewer = function (filename, container) {
    container.html(`<p style="padding:10px; color:white;">'${filename}' 마크다운 파일을 렌더링하는 중...</p>`);

    fetch(`/api/readfile?filename=${encodeURIComponent(filename)}`)
        .then(response => {
            if (!response.ok) throw new Error("파일을 불러오지 못했습니다. (서버 응답: " + response.status + ")");
            return response.text();
        })
        .then(markdownText => {
            const htmlContent = marked.parse(markdownText);
            container.html(`<div class="markdown-body">${htmlContent}</div>`);
        })
        .catch(error => {
            container.html(`<p style="color:red; padding:10px;">${error.message}</p>`);
        });
};