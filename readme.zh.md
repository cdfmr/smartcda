# SmartCDA

[English](readme.md)

老游戏辅助小工具，用于将一些使用CD音轨作为背景音乐的游戏便携化。

## 功能特性

* 模拟音乐CD

  通过播放FLAC/OGG/MP3文件实现MCI接口，模拟音乐CD播放功能, 支持游戏内音量控制。

* 文件重定向

  简易虚拟光驱，将光盘数据文件重定向到硬盘。

* 内存补丁

  修补游戏进程的内存数据，破解其它限制。

## 使用方法

1. 提取CD音轨，编码成FLAC/OGG/MP3文件（命名为track02.ogg、track03.ogg等），复制到游戏目录或其子目录。

2. 若游戏运行时需访问光盘上的文件，复制这些文件到游戏目录或其子目录。

3. 复制smartcda.exe、smartcda.dll、smartcda.ini和audiere.dll文件到游戏目录。

4. 编辑smartcda.ini，根据游戏特征调整设置。

5. 运行smartcda.exe启动游戏。

## 项目依赖

* [Audiere](https://github.com/cdfmr/audiere)

* [Detours](https://github.com/microsoft/Detours)

## 授权协议

Audiere以[LGPL](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html)协议赋予使用者相关权利。

Detours以[MIT](https://github.com/microsoft/Detours/blob/main/LICENSE.md)协议赋予使用者相关权利。

图标文件源自Oxygen Icon Theme，以[LGPL v3](https://www.gnu.org/licenses/lgpl-3.0.html)协议赋予使用者相关权利。

SmartCDA项目的其它代码采用[LGPL v3](https://www.gnu.org/licenses/lgpl-3.0.html)授权协议。如果您需要修改这些代码，并因各种原因不适合采用LGPL授权，请联系我。

## 给我打赏

![](cnpay.png)
