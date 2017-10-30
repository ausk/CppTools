## 中文日历
今天在 [Ubuntu Pacakges](https://packages.ubuntu.com/) 上逛，发现了一个中文日历软件 chinese-calendar.
但是，官网显示的只支持 Qt4，而最近一次更新在2016年。想着挺不错的软件应该支持最新的Qt，遂进行改造。

[为 Chinese-Calendar 增加Qt5支持](https://code.launchpad.net/~jinlj/chinese-calendar/chinese-calendar)

主要方法是

1. 在 pro 文件中增加 widgets 支持。


```pro
    QT += core gui

    greaterThan(4, QT_MAJOR_VERSION): QT += widgets multimedia
    lessThan(5, QT_MAJOR_VERSION): CONFIG += mobility
    lessThan(5, QT_MAJOR_VERSION): MOBILITY += multimedia
```

2. 在源文件或头文件中添加 <QtWidgets> 头文件。

```cpp
    #include <QtGlobal>
    #if QT_VERSION >= 0x050000
    #include <QtWidgets>
    #else
    #include <QtGui>
    #endif
```

3. 有个文件中，缺少头文件，编译时出错后找到并添加 <QDataStream>

4. 更改字符相关

```
//更改：
fromAscii => fromLatin1

//更改：
#if QT_VERSION < 0x050000
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
//    QApplication app(argc, argv);
#endif
```








参考：
(1) [Compiling under Qt4 and Qt5](http://www.qtcentre.org/threads/53731-compiling-under-qt4-AND-qt5)
(2) [How to check QtVersion to include different header](how to check QT_VERSION to include different header?)
(3) [Transition from Qt4.x to Qt5.x](https://wiki.qt.io/Transition_from_Qt_4.x_to_Qt5)
(4) [Automated porting form Qt4 to Qt5](http://www.kdab.com/automated-porting-from-qt-4-to-qt-5/)
