/****************************************************************************
** Meta object code from reading C++ file 'ventoy2diskwindow.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.9.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../ventoy2diskwindow.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ventoy2diskwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.9.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_MyQThread_t {
    QByteArrayData data[5];
    char stringdata0[33];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_MyQThread_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_MyQThread_t qt_meta_stringdata_MyQThread = {
    {
QT_MOC_LITERAL(0, 0, 9), // "MyQThread"
QT_MOC_LITERAL(1, 10, 12), // "thread_event"
QT_MOC_LITERAL(2, 23, 0), // ""
QT_MOC_LITERAL(3, 24, 3), // "msg"
QT_MOC_LITERAL(4, 28, 4) // "data"

    },
    "MyQThread\0thread_event\0\0msg\0data"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MyQThread[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    2,   19,    2, 0x06 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    3,    4,

       0        // eod
};

void MyQThread::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        MyQThread *_t = static_cast<MyQThread *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->thread_event((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (MyQThread::*_t)(int , int );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&MyQThread::thread_event)) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject MyQThread::staticMetaObject = {
    { &QThread::staticMetaObject, qt_meta_stringdata_MyQThread.data,
      qt_meta_data_MyQThread,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *MyQThread::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MyQThread::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MyQThread.stringdata0))
        return static_cast<void*>(const_cast< MyQThread*>(this));
    return QThread::qt_metacast(_clname);
}

int MyQThread::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QThread::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 1)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 1;
    }
    return _id;
}

// SIGNAL 0
void MyQThread::thread_event(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
struct qt_meta_stringdata_Ventoy2DiskWindow_t {
    QByteArrayData data[19];
    char stringdata0[367];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_Ventoy2DiskWindow_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_Ventoy2DiskWindow_t qt_meta_stringdata_Ventoy2DiskWindow = {
    {
QT_MOC_LITERAL(0, 0, 17), // "Ventoy2DiskWindow"
QT_MOC_LITERAL(1, 18, 12), // "thread_event"
QT_MOC_LITERAL(2, 31, 0), // ""
QT_MOC_LITERAL(3, 32, 3), // "msg"
QT_MOC_LITERAL(4, 36, 4), // "data"
QT_MOC_LITERAL(5, 41, 23), // "part_style_check_action"
QT_MOC_LITERAL(6, 65, 8), // "QAction*"
QT_MOC_LITERAL(7, 74, 3), // "act"
QT_MOC_LITERAL(8, 78, 17), // "lang_check_action"
QT_MOC_LITERAL(9, 96, 24), // "on_ButtonInstall_clicked"
QT_MOC_LITERAL(10, 121, 23), // "on_ButtonUpdate_clicked"
QT_MOC_LITERAL(11, 145, 24), // "on_ButtonRefresh_clicked"
QT_MOC_LITERAL(12, 170, 37), // "on_comboBoxDevice_currentInde..."
QT_MOC_LITERAL(13, 208, 5), // "index"
QT_MOC_LITERAL(14, 214, 42), // "on_actionPartition_Configurat..."
QT_MOC_LITERAL(15, 257, 31), // "on_actionClear_Ventoy_triggered"
QT_MOC_LITERAL(16, 289, 33), // "on_actionShow_All_Devices_tog..."
QT_MOC_LITERAL(17, 323, 4), // "arg1"
QT_MOC_LITERAL(18, 328, 38) // "on_actionSecure_Boot_Support_..."

    },
    "Ventoy2DiskWindow\0thread_event\0\0msg\0"
    "data\0part_style_check_action\0QAction*\0"
    "act\0lang_check_action\0on_ButtonInstall_clicked\0"
    "on_ButtonUpdate_clicked\0"
    "on_ButtonRefresh_clicked\0"
    "on_comboBoxDevice_currentIndexChanged\0"
    "index\0on_actionPartition_Configuration_triggered\0"
    "on_actionClear_Ventoy_triggered\0"
    "on_actionShow_All_Devices_toggled\0"
    "arg1\0on_actionSecure_Boot_Support_triggered"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_Ventoy2DiskWindow[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
      11,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    2,   69,    2, 0x08 /* Private */,
       5,    1,   74,    2, 0x08 /* Private */,
       8,    1,   77,    2, 0x08 /* Private */,
       9,    0,   80,    2, 0x08 /* Private */,
      10,    0,   81,    2, 0x08 /* Private */,
      11,    0,   82,    2, 0x08 /* Private */,
      12,    1,   83,    2, 0x08 /* Private */,
      14,    0,   86,    2, 0x08 /* Private */,
      15,    0,   87,    2, 0x08 /* Private */,
      16,    1,   88,    2, 0x08 /* Private */,
      18,    0,   91,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    3,    4,
    QMetaType::Void, 0x80000000 | 6,    7,
    QMetaType::Void, 0x80000000 | 6,    7,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   13,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   17,
    QMetaType::Void,

       0        // eod
};

void Ventoy2DiskWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Ventoy2DiskWindow *_t = static_cast<Ventoy2DiskWindow *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->thread_event((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 1: _t->part_style_check_action((*reinterpret_cast< QAction*(*)>(_a[1]))); break;
        case 2: _t->lang_check_action((*reinterpret_cast< QAction*(*)>(_a[1]))); break;
        case 3: _t->on_ButtonInstall_clicked(); break;
        case 4: _t->on_ButtonUpdate_clicked(); break;
        case 5: _t->on_ButtonRefresh_clicked(); break;
        case 6: _t->on_comboBoxDevice_currentIndexChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 7: _t->on_actionPartition_Configuration_triggered(); break;
        case 8: _t->on_actionClear_Ventoy_triggered(); break;
        case 9: _t->on_actionShow_All_Devices_toggled((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 10: _t->on_actionSecure_Boot_Support_triggered(); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QAction* >(); break;
            }
            break;
        case 2:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QAction* >(); break;
            }
            break;
        }
    }
}

const QMetaObject Ventoy2DiskWindow::staticMetaObject = {
    { &QMainWindow::staticMetaObject, qt_meta_stringdata_Ventoy2DiskWindow.data,
      qt_meta_data_Ventoy2DiskWindow,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *Ventoy2DiskWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Ventoy2DiskWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_Ventoy2DiskWindow.stringdata0))
        return static_cast<void*>(const_cast< Ventoy2DiskWindow*>(this));
    return QMainWindow::qt_metacast(_clname);
}

int Ventoy2DiskWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
