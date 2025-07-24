document.addEventListener('DOMContentLoaded', () => {
  fetch('./nav.html')
    .then(response => response.text())
    .then(html => {
      const tempDiv = document.createElement('div');
      tempDiv.innerHTML = html;

      const topbarContent = tempDiv.querySelector('.top-bar');
      const sidebarContent = tempDiv.querySelector('.sidebar');
      const footerContent = tempDiv.querySelector('.footer');

      if (topbarContent) document.querySelector('.top-bar').innerHTML = topbarContent.innerHTML;
      if (sidebarContent) document.querySelector('.sidebar').innerHTML = sidebarContent.innerHTML;
      if (footerContent) document.querySelector('.footer').innerHTML = footerContent.innerHTML;

      // Sidebar toggle
      const toggleMenuBtn = document.querySelector('.toggle-menu');
      const sidebar = document.querySelector('.sidebar');

      if (toggleMenuBtn && sidebar) {
        toggleMenuBtn.addEventListener('click', (e) => {
          e.stopPropagation();
          sidebar.classList.toggle('open');
        });
      }

      document.addEventListener('click', (e) => {
        if (!e.target.closest('.sidebar') && !e.target.closest('.toggle-menu')) {
          sidebar.classList.remove('open');
        }
      });

      // Active link logic (works with GitHub Pages)
      const currentFile = window.location.pathname.toLowerCase().split('/').pop() || 'index.html';

      const links = document.querySelectorAll('.sidebar a');
      links.forEach(link => link.classList.remove('active'));

      links.forEach(link => {
        try {
          const linkHref = link.getAttribute('href').toLowerCase().replace('./', '').split('?')[0].split('#')[0];

          if (
            linkHref === currentFile ||
            (currentFile === 'index.html' && (linkHref === '' || linkHref === 'index.html'))
          ) {
            link.classList.add('active');
          }
        } catch (e) {
          console.warn('Invalid link href:', link.href);
        }
      });

      // Theme toggle
      const toggleThemeBtn = document.querySelector('.toggle-theme');
      const body = document.body;
      const savedTheme = localStorage.getItem('theme');
      if (savedTheme === 'dark') {
        body.classList.add('darkmode');
      }
      if (toggleThemeBtn) {
        toggleThemeBtn.addEventListener('click', () => {
          body.classList.toggle('darkmode');
          localStorage.setItem('theme', body.classList.contains('darkmode') ? 'dark' : 'light');
        });
      }
    })
    .catch(err => {
      console.error('Failed to load nav.html:', err);
    });
});
