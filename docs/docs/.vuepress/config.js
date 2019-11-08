module.exports = {
  head: [
    ['link', { rel: 'icon', type: 'image/png', href: '/app-logo-128x128.png' }],
    ['link', { rel: 'icon', type: 'image/png', sizes: '16x16', href: '/icons/favicon-16x16.png' }],
    ['link', { rel: 'icon', type: 'image/png', sizes: '32x32', href: '/icons/favicon-32x32.png' }],
    ['link', { rel: 'icon', type: 'image/png', sizes: '96x96', href: '/icons/favicon-96x96.png' }],
    ['link', { rel: 'icon', type: 'image/ico', href: '/icons/favicon.ico' }],
    // PWA
    // ['link', { rel: 'icon', href: '/hyperion-logo-white.png' }], // ???
    ['link', { rel: 'manifest', href: '/manifest.json' }], // ???
    ['meta', { name: 'theme-color', content: '#2b81a0' }], // ???
    ['meta', { name: 'apple-mobile-web-app-capable', content: 'yes' }],
    ['meta', { name: 'apple-mobile-web-app-status-bar-style', content: 'black' }],
    ['link', { rel: 'apple-touch-icon', href: '/icons/apple-icon-152x152.png' }],
    ['link', { rel: 'mask-icon', href: '/icons/safari-pinned-tab.svg', color: '#fff' }],
    ['meta', { name: 'msapplication-TileImage', content: '/icons/ms-icon-144x144.png' }], // probably invert
    ['meta', { name: 'msapplication-TileColor', content: '#2b81a0' }] // might not match with icon
  ],
  dest: "./dist",
  title: "Hyperion",
  // removing this sections disables lang selector. But everything else works. Featue Request to merge with themeConfig.locales?
  locales: {
    "/": {
      lang: 'en-US',
      // Description of page does not work inside themeConfig
      description: 'Hyperion Ambient Light documentation'
    },
    "/de/": {
      lang: 'de-DE',
      description: 'Hyperion Ambient Light Dokumentation'
    }
  },
  themeConfig: {
    /*  algolia: {
          apiKey: '<API_KEY>',
          indexName: '<INDEX_NAME>'
        },
        
    */
    sidebarDepth: 3,
    smoothScroll: true,
    logo: '/hyperion-logo.png',
    locales: {
      '/': {
        // text for the language dropdown
        selectText: 'Languages',
        // label for this locale in the language dropdown
        label: 'English',
        // Aria Label for locale in the dropdown
        ariaLabel: 'Languages',
        // text for last updated
        lastUpdated: 'Last Updated',
        // Seach placeholder
        searchPlaceholder: 'Search...',
        // Customising the header label
        repoLabel: 'Contribute',
        // custom text for edit link. Defaults to "Edit this page"
        editLinkText: 'Edit this page on GitHub',
        // config for Service Worker
        serviceWorker: {
          updatePopup: {
            message: "New content is available.",
            buttonText: "Refresh"
          }
        },
        // The top navbar
        nav: [
          { text: 'Home', link: '/' },
          { text: 'User', link: '/en/user/' },
          { text: 'Effects', link: '/en/effects/' },
          { text: 'Json API', link: '/en/json/' }
          // { text: 'Addons API', link: '/en/addons/' }
        ],
        sidebar: {
          '/en/user/': getUserSidebar('General', 'Advanced'),
          '/en/effects/': getEffectsSidebar('Effects', 'Effects UI'),
          '/en/json/': getJsonSidebar('JSON', 'Misc'),
          '/en/api/': getApiSidebar('Go back to')
          //   '/en/addons/': getAddonsSidebar('Addons', 'Addons UI')
        }
      },
      '/de/': {
        selectText: 'Sprachen',
        label: 'Deutsch',
        searchPlaceholder: 'Wer sucht...',
        lastUpdated: 'Zuletzt Aktualisiert',
        repoLabel: 'Mach mit',
        editLinkText: 'Editiere diese Seite auf Github',
        serviceWorker: {
          updatePopup: {
            message: "Neuer Inhalt ist verfügbar.",
            buttonText: "Aktualisieren"
          }
        },
        nav: [
          { text: 'Startseite', link: '/de/' }
          //{ text: 'Benutzer', link: '/de/user/' }
          //{ text: 'Effects', link: '/en/effects/' },
          //{ text: 'Json API', link: '/en/json/' }
          // { text: 'Addons API', link: '/en/addons/' }
        ],
        // '/de/user/': getUserSidebar('Allgemein', 'Erweitert'),
        // '/de/effects/': getEffectsSidebar('Effekte', 'Effekt UI'),
        // '/de/json/': getJsonSidebar('JSON', 'Misc'),
        // '/de/api/': getApiSidebar('Gehe zurück zu')
        //   '/en/addons/': getAddonsSidebar('Addons', 'Addons UI')
      }
    },
    // Customising the header label
    // Defaults to "GitHub"/"GitLab"/"Bitbucket" depending on `themeConfig.repo`
    repoLabel: 'Contribute',
    // Optional options for generating "Edit this page" link
    // if your docs are in a different repo from your main project:
    docsRepo: 'hyperion-project/hyperion.ng',
    // if your docs are not at the root of the repo:
    docsDir: 'docs/docs',
    // if your docs are in a specific branch (defaults to 'master'):
    docsBranch: 'master',
    // defaults to false, set to true to enable
    editLinks: true,
    // default value is true. Allows to hide next page links on all pages
    nextLinks: true,
    // default value is true. Allows to hide prev page links on all pages
    prevLinks: true,
    // custom text for edit link. Defaults to "Edit this page"
    //editLinkText: 'Edit this page on GitHub'
  },
  plugins: [

    // https://vuepress.github.io/en/plugins/medium-zoom/
    [
      'vuepress-plugin-medium-zoom',
      {
        selector: '.zoomable',
        delay: 1000,
        options: {
          margin: 24,
          background: '#000000bf',
          scrollOffset: 0,
        }
      }
    ],
    // https://vuepress.github.io/en/plugins/redirect
    [
      'vuepress-plugin-redirect',
      {
        locales: true
      }
    ],
    // https://vuepress.vuejs.org/plugin/official/plugin-pwa.html#install
    [
      '@vuepress/pwa', {
        serviceWorker: true,
        updatePopup: true
      }
    ],
    // https://v1.vuepress.vuejs.org/plugin/official/plugin-back-to-top.html
    [
      '@vuepress/back-to-top'
    ],
    [
      'vuepress-plugin-serve'
    ]
  ]
}

function getUserSidebar (groupA, groupB) {
  return [
    {
      title: groupA,
      collapsable: false,
      children: [
        '',
        'Installation',
        'Configuration',
        'LedDevices',
        'HyperBian',
      ]
    },
    {
      title: groupB,
      collapsable: false,
      children: [
        'advanced/Advanced',
        'advanced/Support',
      ]
    }
  ]
}

function getEffectsSidebar (groupA, groupB) {
  return [{
    title: groupA,
    collapsable: false,
    children: [
      '',
      'API',
      'OurFirstEffect',
    ]
  },
  {
    title: groupB,
    collapsable: false,
    children: [
      '../api/Ui'
    ]
  }
  ]
}

function getJsonSidebar (groupA, groupB) {
  return [{
    title: groupA,
    collapsable: false,
    children: [
      '',
      'ServerInfo',
      'Control',
      'Authorization',
      'Subscribe',
    ]
  },
  {
    title: groupB,
    collapsable: false,
    children: [
      '../api/Detect',
      '../api/Guidelines'
    ]
  }
  ]
}

function getApiSidebar (groupA) {
  return [
    {
      title: groupA,
      collapsable: false,
      children: [
        '../effects/',
        '../json/'
      ]
    }
  ]
}

function getAddonsSidebar (groupA, groupB) {
  return [{
    title: groupA,
    collapsable: false,
    children: [
      '',
      'API',
      'OurFirstAddon',
    ]
  },
  {
    title: groupB,
    collapsable: false,
    children: [
      '../api/Ui'
    ]
  }
  ]
}
