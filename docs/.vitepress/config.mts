import { defineConfig } from 'vitepress'

// Sparks 固件文档（v1.0.0）
// GitHub Pages 项目站 base 须与仓库名一致（非 ./，VitePress 不支持相对 base）
const base = process.env.GITHUB_ACTIONS ? '/m5stack-cardputer-sparks/' : '/'

export default defineConfig({
  title: 'Sparks',
  description: 'M5Stack Cardputer 多应用固件文档',
  lang: 'zh-CN',
  base,
  lastUpdated: true,
  cleanUrls: true,

  vite: {
    server: { port: 3123 },
    preview: { port: 3123 },
  },

  head: [
    ['link', { rel: 'icon', href: `${base}assets/logo_60.png` }],
  ],

  themeConfig: {
    logo: '/assets/logo_60.png',
    siteTitle: 'Sparks',
    nav: [
      { text: '首页', link: '/' },
      { text: '功能目录', link: '/apps/' },
      { text: '截图', link: '/apps/shots' },
      { text: '快捷键', link: '/guide/shortcuts' },
      {
        text: 'v1.0.1',
        items: [
          { text: '入门', link: '/guide/getting-started' },
          { text: 'CHANGELOG', link: 'https://github.com/KyleBing/m5stack-cardputer-sparks/blob/main/CHANGELOG.md' },
        ],
      },
    ],

    sidebar: [
      {
        text: '开始',
        items: [
          { text: '简介', link: '/' },
          { text: '入门', link: '/guide/getting-started' },
          { text: '全局快捷键', link: '/guide/shortcuts' },
        ],
      },
      {
        text: '功能目录',
        items: [
          { text: '总览', link: '/apps/' },
          { text: '截图总览', link: '/apps/shots' },
        ],
      },
      {
        text: '米家控制',
        items: [
          { text: 'Mijia 米家', link: '/apps/mijia' },
          { text: '米家设备 Token 获取', link: '/apps/mijia-token' },
          { text: 'Infrared 红外', link: '/apps/infrared' },
        ],
      },
      {
        text: '时间与电源',
        items: [
          { text: 'Time 时间', link: '/apps/time' },
          { text: 'Battery 电池', link: '/apps/battery' },
          { text: 'Sleep 睡眠', link: '/apps/sleep' },
          { text: 'Cursor 用量', link: '/apps/cursor' },
          { text: 'Keyboard', link: '/apps/hid-keyboard' },
          { text: 'Morse 摩斯', link: '/apps/morse' },
        ],
      },
      {
        text: '系统与信息',
        items: [
          { text: 'Config 配网', link: '/apps/config' },
          { text: 'WiFi', link: '/apps/wifi' },
          { text: 'BLE', link: '/apps/ble' },
          { text: 'Options 选项', link: '/apps/options' },
          { text: 'Info 信息', link: '/apps/info' },
          { text: 'Version 版本', link: '/apps/version' },
        ],
      },
      {
        text: '硬件调试与演示',
        items: [
          { text: 'IMU', link: '/apps/imu' },
          { text: 'RGB LED', link: '/apps/rgb-led' },
          { text: 'Mic 麦克风', link: '/apps/mic' },
          { text: 'Display 显示', link: '/apps/display' },
          { text: 'Icons 图标', link: '/apps/icons' },
          { text: 'Font 字体', link: '/apps/font' },
          { text: 'I2C 扫描', link: '/apps/i2c' },
        ],
      },
      {
        text: '开发',
        items: [
          { text: '图片处理与烘焙', link: '/dev/images' },
          { text: '内存说明', link: '/dev/memory' },
        ],
      },
    ],

    socialLinks: [
      { icon: 'github', link: 'https://github.com/KyleBing/m5stack-cardputer-sparks' },
    ],

    search: {
      provider: 'local',
    },

    outline: {
      label: '本页目录',
      level: [2, 3],
    },

    docFooter: {
      prev: '上一篇',
      next: '下一篇',
    },

    lastUpdated: {
      text: '最后更新',
    },

    footer: {
      message: 'Sparks for M5Stack Cardputer',
      copyright: 'v1.0.0 · KyleBing',
    },
  },
})
