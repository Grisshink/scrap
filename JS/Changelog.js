function showSectionByHash() {
    const hash = window.location.hash.substring(1);
    const allSections = document.querySelectorAll("main > section");

    // Always hide all sections first
    allSections.forEach(section => {
        section.style.display = "none";
    });

    // Only show the section that matches the hash
    if (hash) {
        const matched = document.getElementById(hash);
        if (matched) {
            matched.style.display = "block";
        }
    }
}

document.addEventListener("DOMContentLoaded", showSectionByHash);
window.addEventListener("hashchange", showSectionByHash);
