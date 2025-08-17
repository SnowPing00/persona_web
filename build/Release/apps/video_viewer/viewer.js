window.personaApps = window.personaApps || {};

window.personaApps.renderVideoViewer = function (filename, container) {
    container.html(`
        <div style="width: 100%; height: 100%; background-color: black; display: flex; align-items: center; justify-content: center;">
            <video controls autoplay style="max-width: 100%; max-height: 100%;">
                <source src="/api/streamfile?filename=${encodeURIComponent(filename)}" type="video/mp4">
                브라우저가 비디오 태그를 지원하지 않습니다.
            </video>
        </div>
    `);
};