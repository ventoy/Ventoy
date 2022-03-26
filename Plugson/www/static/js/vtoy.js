
function ventoy_replace_slash(str) {
    var str1 = str.replace(/\\/g, '/');
    var str2 = str1.replace(/\/\//g, '/');
    return str2;
}


function ventoy_get_ulen(str) {    
    var c, b = 0, l = str.length;    
    while(l) {    
        c = str.charCodeAt(--l);    
        b += (c < 128) ? 1 : ((c < 2048) ? 2 : ((c < 65536) ? 3 : 4));    
    };    
    return b;    
}


function ventoy_common_check_path(path) {
    if (path.indexOf('//') >= 0) {
        return false;
    }

    if (path.length <= g_current_dir.length) {
        return false;
    }

    if (path.substr(0, g_current_dir.length) != g_current_dir) {
        return false;
    }

    return true;
}


// 加载子页面内容
function loadContent(page) {
    $("#plugson-content").load(page + ".html?_=" + (new Date().getTime()), function() {
        $("body").scrollTop(0);
    });
}

// 包装ajax请求
function callVtoyCatchErr(p1, p2, p3) {
    var url = '/vtoy/json';
    var data = {};
    var func = function(data) {};
    var errfunc = function(xmlHttpRequest, textStatus, errorThrown) {};

    if (typeof(p1) === 'string') {
        url = p1;
    } else if (typeof(p1) === 'object') {
        data = p1;
    }
    if (typeof(p2) === 'object') {
        data = p2;
    } else if (typeof(p2) === 'function') {
        func = p2;
    }
    if (typeof(p3) === 'function') {
        errfunc = p3;
    }

    //vtoy.debug('callVtoy:\t\t\t\t' + JSON.stringify(data));
    $.ajax({
        url: url,
        type: 'post',
        cache: false,
        dataType: 'json',
        data: JSON.stringify(data),
        success: func,
        error: errfunc,
        complete: function(data) {
        //vtoy.debug('callVtoy\'s resp:\t\t' + data.responseText);
        }
    });
}

// 包装ajax请求
function callVtoy(p1, p2, p3) {
    var url = '/vtoy/json';
    var data = {};
    var func = function(data) {};

    if (typeof(p1) === 'string') {
        url = p1;
    } else if (typeof(p1) === 'object') {
        data = p1;
    }
    if (typeof(p2) === 'object') {
        data = p2;
    } else if (typeof(p2) === 'function') {
        func = p2;
    }
    if (typeof(p3) === 'function') {
        func = p3;
    }

    //vtoy.debug('callVtoy:\t\t\t\t' + JSON.stringify(data));
    $.ajax({
        url: url,
        type: 'post',
        cache: false,
        dataType: 'json',
        data: JSON.stringify(data),
        success: func,
        error: function(xmlHttpRequest, textStatus, errorThrown) {

            if(undefined === errorThrown)
            {
                Message.error(g_vtoy_cur_language.STR_WEB_REMOTE_ABNORMAL);
            }
            else if(undefined === errorThrown.length)
            {
                
            }
            else if('' == errorThrown.trim())
            {
            }
            else
            {
                switch(errorThrown)
                {
                    case 'timeout':
                    {
                        Message.error(g_vtoy_cur_language.STR_WEB_REQUEST_TIMEOUT);
                        break;
                    }
                    case 'Service Unavailable':
                    {
                        Message.error(g_vtoy_cur_language.STR_WEB_SERVICE_UNAVAILABLE);
                        break;
                    }
                    case 'abort':
                    {
                        break;
                    }
                    default:
                    {
                        Message.error(g_vtoy_cur_language.STR_WEB_COMMUNICATION_ERR + errorThrown);
                        break;
                    }
                }
            }
        },
        complete: function(data) {
        //vtoy.debug('callVtoy\'s resp:\t\t' + data.responseText);
        }
    });
}

function callVtoyASyncTimeout(time, data, func) {
    $.ajax({
        url: '/vtoy/json',
        type: 'post',
        cache: false,
        dataType: 'json',
        async: true,
        timeout: time,
        data: JSON.stringify(data),
        success: func,
        error: function(xmlHttpRequest, textStatus, errorThrown) {
            if(undefined === errorThrown)
            {                
            }
            else if(undefined === errorThrown.length)
            {
                
            }
            else if('' == errorThrown.trim())
            {
            }
            else
            {
                switch(errorThrown)
                {
                    case 'timeout':
                    {
                        callVtoyASyncTimeout(time, data, func);
                        break;
                    }
                    case 'Service Unavailable':
                    {
                        break;
                    }
                    case 'abort':
                    {
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }
        },
        complete: function(data) {
            //vtoy.debug('callVtoyASyncTimeout\'s resp:\t' + JSON.stringify(data));
        }
    });
}

function callVtoySync(data, func) {
    //vtoy.debug('callVtoySync:\t\t\t' + JSON.stringify(data));
    $.ajax({
        url: '/vtoy/json',
        type: 'post',
        cache: false,
        dataType: 'json',
        async: false,
        data: JSON.stringify(data),
        success: function VtoyCallFuncWrapper(data) {            
            if (data.result === 'busy') {
                var titlestr = '<span class="fa fa-check-circle" style="color:green; font-weight:bold;"> ' + g_vtoy_cur_language.STR_INFO + '</span>';
                var msgstr = '<span style="font-size:14px; font-weight:bold;"> ' + g_vtoy_cur_language.STR_WEB_SERVICE_BUSY + '</span>';
                Modal.alert({title:titlestr, msg:msgstr, btnok:g_vtoy_cur_language.STR_BTN_OK });                
            }else {
                func(data);
            }
        },
        error: function(xmlHttpRequest, textStatus, errorThrown) {
            if(undefined === errorThrown)
            {
                Message.error(g_vtoy_cur_language.STR_WEB_REMOTE_ABNORMAL);
            }
            else if(undefined === errorThrown.length)
            {
                
            }
            else if('' == errorThrown.trim())
            {
            }
            else
            {
                switch(errorThrown)
                {
                    case 'timeout':
                    {
                        Message.error(g_vtoy_cur_language.STR_WEB_REQUEST_TIMEOUT);
                        break;
                    }
                    case 'Service Unavailable':
                    {
                        Message.error(g_vtoy_cur_language.STR_WEB_SERVICE_UNAVAILABLE);
                        break;
                    }
                    case 'abort':
                    {
                        break;
                    }
                    default:
                    {
                        Message.error(g_vtoy_cur_language.STR_WEB_COMMUNICATION_ERR + errorThrown);
                        break;
                    }
                }
            }
        },
        complete: function(data) {
            //vtoy.debug('callVtoySync\'s resp:\t' + JSON.stringify(data));
        }
    });
}

var vtoy = {    
    baseurl : '',
    status: '',    
    scan: {
        time: 3,
        ret: []
    }
}

// 
String.prototype.endsWith = function(str) {
    if (str == null || str == "" || this.length == 0 || str.length > this.length)
        return false;
    if (this.substring(this.length - str.length) == str)
        return true;
    else
        return false;
}

window.Message = function() {
    var _showMsg = function(type, msg, time) {
        var o = {type : type, msg : msg };
        if(time) {
            o.time = time;
        }
        _show(o);
    }
    
    var _show = function(options) {
        var ops = {
            msg : "提示内容",
            type: 'S',
            time: 3000
        };
        $.extend(ops, options);

        var msg_class = 'alert-success';
        if('S' === ops.type || 's' === ops.type) {
            msg_class = 'alert-success';
        } else if ('E' === ops.type || 'e' === ops.type) {
            msg_class = 'alert-danger';
        } else if ('W' === ops.type || 'w' === ops.type) {
            msg_class = 'alert-warning';
        } else if ('I' === ops.type || 'i' === ops.type) {
            msg_class = 'alert-info';
        } else {
            alert("未知的类型，请使用: w-警告;s-成功;e-失败;i-提示");
            return;
        }
        var $messageContainer = $("#fcss_message");
        if($messageContainer.length === 0) {
            $messageContainer = $('<div id="fcss_message" style="position:fixed; left: 20%; right: 20%; top:0px; z-index:99999999"></div>');
            $messageContainer.appendTo($('body'));
        }
        var $div = $('<div class="alert ' + msg_class + ' alert-dismissible fade in" role="alert" style="margin-bottom: 0; padding-top:10px; padding-bottom: 10px;"></div>');
        var $btn = $('<button type="button" class="close" data-dismiss="alert" aria-label="Close"><span aria-hidden="true">×</span></button>');
        $div.append($btn).append(ops.msg).appendTo($messageContainer);
        setTimeout(function() {
            $div.remove();
        }, ops.time);
    }
    
    var _success = function(msg, time) {
        _showMsg('s', msg, time);
    }
    var _error = function(msg, time) {        
        _showMsg('e', msg, time || 5000);
    }
    var _warn = function(msg, time) {
        _showMsg('w', msg, time);
    }
    var _info = function(msg, time) {
        _showMsg('i', msg, time);
    }
    
    return {
        success : _success,
        error	: _error,
        warn 	: _warn,
        info    : _info,
        show	: _show
    }
}();


var g_vtoy_cur_language_en = 
{
    "STR_INFO": "Info",
    "STR_BTN_OK": "OK",
    "STR_BTN_CANCEL": "Cancel",
    "STR_SAVE": " Save",
    "STR_RESET": " Reset",
    "STR_DISCARD": " Discard",
    "STR_ENABLE": " Enable",
    "STR_ADD": "Add",
    "STR_DEL": "Delete",
    "STR_CLEAR": "Clear",
    "STR_STATUS": "Status",
    "STR_DEFAULT": "Default",
    "STR_DEFAULT_SEL": "Default",
    "STR_RANDOM_SEL": "Random",
    "STR_OPERATION": "Operation",
    "STR_VALID": "Valid",
    "STR_INVALID": "Invalid",
    "STR_OPT_SETTING": "Option Setting",
    "STR_OPT_DESC": "Option Description",
    "STR_EDIT": "Edit",
    "STR_FILE": "File",
    "STR_DIR": "Dir",
    "STR_SAVE_TIP": "Data in current page has been modified. Do you want to save it?",
    "STR_SAVE_SUCCESS": "Configuration successfully saved!",
    "STR_FILE_EXIST": "OK",
    "STR_FILE_NONEXIST": "Invalid",
    "STR_FILE_FUZZY": "Fuzzy",
    "STR_DIR_EXIST": "OK",
    "STR_DIR_NONEXIST": "Invalid",
    "STR_DUPLICATE_PATH": "Duplicate path",
    "STR_DELETE_CONFIRM": "Delete this item, Continue?",
    "STR_FILE_PATH": "File Path",
    "STR_DIR_PATH": "Directory Path",

    "STR_SET_ALIAS": "Set Menu Alias",
    "STR_ALIAS": "Alias",
    "STR_SET_TIP": "Set Menu Tip",
    "STR_TIP": "Tip",
    "STR_SET_CLASS": "Set Menu Class",
    "STR_CLASS": "Class",
    "STR_SET_INJECTION": "Set Injection",
    "STR_SET_AUTO_INS": "Set Auto Install",
    "STR_SET_AUTO_TEMPLATE": "Template",

    "STR_SET_PERSISTENCE": "Set Persistence",
    "STR_SET_PERSISTENCE_DAT": "Dat File",

    "STR_SET_DUD": "Set DUD",
    "STR_SET_DUD_FILE": "DUD File",

    "STR_PASSWORD": "Password",
    "STR_SET_PASSWORD": "Set Password",
    "STR_PASSWORD_TYPE": "Password Type",
    "STR_PASSWORD_VALUE": "Password Value",


    "STR_WEB_COMMUNICATION_ERR":"Communication error:",
    "STR_WEB_REMOTE_ABNORMAL":"Communication error: remote abnormal",
    "STR_WEB_REQUEST_TIMEOUT":"Communication error: Request timed out",
    "STR_WEB_SERVICE_UNAVAILABLE":"Communication error: Service Unavailable",
    "STR_WEB_SERVICE_BUSY":"Service is busy, please retry later.",

    "STR_PLUG_DEVICE": "Device Information",
    "STR_PLUG_CONTROL": "Global Control Plugin",
    "STR_PLUG_THEME": "Theme Plugin",
    "STR_PLUG_ALIAS": "Menu Alias Plugin",
    "STR_PLUG_CLASS": "Menu Class Plugin",
    "STR_PLUG_TIP": "Menu Tip Plugin",
    "STR_PLUG_AUTO_INSTALL": "Auto Install Plugin",
    "STR_PLUG_PERSISTENCE": "Persistence Plugin",
    "STR_PLUG_INJECTION": "Injection Plugin",
    "STR_PLUG_CONF_REPLACE": "Boot Conf Replace Plugin",
    "STR_PLUG_PASSWORD": "Password Plugin",
    "STR_PLUG_IMAGELIST": "Image List Plugin",
    "STR_PLUG_AUTO_MEMDISK": "Auto Memdisk Plugin",
    "STR_PLUG_DUD": "DUD Plugin",
    "STR_PLUG_DONATION": "Donation",

    "STR_PATH_TOO_LONG": "The path exceeds the maximum supported length, please check!",
    "STR_INPUT_TOO_LONG": "The string exceeds the maximum supported length, please check!",
    "STR_INVALID_FILE_PATH": "Invalid or nonexist full file path, please check!",
    "STR_INVALID_FILE_PATH1": "The 1st file path is invalid or nonexist!",    
    "STR_INVALID_FILE_PATH2": "The 2nd file path is invalid or nonexist!",    
    "STR_INVALID_NEW_FILE_PATH": "The full file path of new is invalid or nonexist, please check!",
    "STR_INVALID_DIR_PATH": "Invalid directory path, please check!",
    "STR_INVALID_NUMBER": "Please input valid non-negative integer!",
    "STR_INVALID_AUTOSEL": "autosel exceeds the length of the list!",
    "STR_INVALID_TIMEOUT": "Please input valid timeout integer!",
    "STR_INVALID_PERCENT": "Please input integer between 0-100 !",
    "STR_INVALID_COLOR": "Please input valid color!",
    "STR_SELECT": "Please Select",
    "STR_SET_ALIAS_FOR_FILE": "Set Menu Alias For File",
    "STR_SET_ALIAS_FOR_DIR": "Set Menu Alias For Directory",
    "STR_SET_TIP_FOR_FILE": "Set Menu Tip For File",
    "STR_SET_TIP_FOR_DIR": "Set Menu Tip For Directory",
    "STR_SET_INJECTION_FOR_FILE": "[image] Set injection for a file",
    "STR_SET_INJECTION_FOR_DIR": "[parent] Set the same injection for all the files under a directory.",
    "STR_INVALID_ARCHIVE_PATH": "Invalid or nonexist archive file path, please check!",
    "STR_SET_PWD_FOR_FILE": "[file] Set password for a file.",
    "STR_SET_PWD_FOR_DIR": "[parent] Set the same password for all the files under a directory.",
    "STR_SET_AUTO_INSTALL_FOR_FILE": "[image] Set auto install template for a file",
    "STR_SET_AUTO_INSTALL_FOR_DIR": "[parent] Set the same auto install template for all the files under a directory.",

    "STR_SET_CLASS_BY_KEY": "[key] Set menu class by filename keyword.",
    "STR_SET_CLASS_BY_DIR": "[dir] Set menu class for a directory.",
    "STR_SET_CLASS_BY_PARENT": "[parent] Set menu class for all the files under a directory.",
    "STR_SET_IMAGE_PWD": "[file] Set Password For A File",
    "STR_SET_PARENT_PWD": "[parent] Set the same password for all the files under a directory.",

    "STR_SET_SEARCH_ROOT": "Set Search Root",    
    "STR_SET_DEFAULT_IMAGE": "Set Default Image",
    "STR_ADD_THEME": "Add Theme",
    "STR_ADD_FONT": "Set Font",
    "STR_ADD_FILE_TO_LIST": "Add File To List",
    "STR_DEFAULT_SELECT": " Default",
    "STR_AUTO_TEMPLATE": "Auto Install Template",
    "STR_ADD_AUTO_TEMPLATE": "Add Auto Install Template",
    
    "STR_PERSISTENCE_DAT": "Persistence Dat File",
    "STR_ADD_PERSISTENCE_DAT": "Add Persistence Dat File",

    "STR_DUD_FILE": "DUD File",
    "STR_ADD_DUD_FILE": "Add DUD File",

    "STR_DEL_LAST": "The entry will be deleted if you delete the this last item. Continue?",

    "STR_CLOSE_TIP": "Service unavailable, the page will close!",
    "STR_SECURE_BOOT_ENABLE": "Enable",
    "STR_SECURE_BOOT_DISABLE": "Disable",
    "STR_SYNTAX_ERROR_TIP": "Syntax error detected in ventoy.json, so the configuration is not loaded!",
    "STR_INVALID_CONFIG_TIP": "Invalid configuration detected in ventoy.json, so the configuration is not loaded!",
    "STR_CONFIG_SAVE_ERROR_TIP": "Failed to write ventoy.json file. Check VentoyPlugson.log for more details!",

    "STR_XXX": "xxx"
};

var g_vtoy_cur_language_cn = 
{
    "STR_INFO": "提醒",
    "STR_BTN_OK": "确定",
    "STR_BTN_CANCEL": "取消",
    "STR_SAVE": " 保存",
    "STR_RESET": " 重置",
    "STR_DISCARD": " 丢弃",
    "STR_ENABLE": " 使能",
    "STR_ADD": "新增",
    "STR_DEL": "删除",
    "STR_CLEAR": "清除",
    "STR_STATUS": "状态",
    "STR_DEFAULT": "默认",
    "STR_DEFAULT_SEL": "默认选择",
    "STR_RANDOM_SEL": "随机选择",
    "STR_OPERATION": "操作",
    "STR_VALID": "有效",
    "STR_INVALID": "无效",
    "STR_OPT_SETTING": "选项设置",
    "STR_OPT_DESC": "选项说明",
    "STR_EDIT": "设置",
    "STR_FILE": "文件",
    "STR_DIR": "目录",
    "STR_SAVE_TIP": "当前页面数据已经修改，是否保存？",
    "STR_SAVE_SUCCESS": "配置保存成功！",
    "STR_FILE_EXIST": "文件存在",
    "STR_FILE_NONEXIST": "文件无效",
    "STR_FILE_FUZZY": "模糊匹配",
    "STR_DIR_EXIST": "目录存在",
    "STR_DIR_NONEXIST": "目录无效",
    "STR_DUPLICATE_PATH": "路径不允许重复",
    "STR_DELETE_CONFIRM": "确定要删除吗？",
    "STR_FILE_PATH": "文件路径",
    "STR_DIR_PATH": "目录路径",
    
    "STR_SET_ALIAS": "设置菜单别名",
    "STR_ALIAS": "别名",
    "STR_SET_TIP": "设置菜单提示",
    "STR_TIP": "提示",
    "STR_SET_CLASS": "设置菜单类型",
    "STR_CLASS": "类型",
    "STR_SET_INJECTION": "设置文件注入",
    "STR_SET_AUTO_INS": "设置自动安装",
    "STR_SET_AUTO_TEMPLATE": "自动安装脚本",
    "STR_SET_PERSISTENCE": "设置持久化文件",
    "STR_SET_PERSISTENCE_DAT": "持久化文件",

    "STR_SET_DUD": "设置 DUD",
    "STR_SET_DUD_FILE": "DUD 文件",

    "STR_PASSWORD": "密码",
    "STR_SET_PASSWORD": "设置密码",
    "STR_PASSWORD_TYPE": "密码类型",
    "STR_PASSWORD_VALUE": "密码内容",

    "STR_WEB_COMMUNICATION_ERR":"通信失败：",
    "STR_WEB_REMOTE_ABNORMAL":"通信失败：服务端异常",
    "STR_WEB_REQUEST_TIMEOUT":"通信失败：请求超时",
    "STR_WEB_SERVICE_UNAVAILABLE":"通信失败：服务不可用",
    "STR_WEB_SERVICE_BUSY":"后台服务正忙，请稍后重试",

    "STR_PLUG_DEVICE": "设备信息",
    "STR_PLUG_CONTROL": "全局控制插件",
    "STR_PLUG_THEME": "主题插件",
    "STR_PLUG_ALIAS": "菜单别名插件",
    "STR_PLUG_CLASS": "菜单类型插件",
    "STR_PLUG_TIP": "菜单提示插件",
    "STR_PLUG_AUTO_INSTALL": "自动安装插件",
    "STR_PLUG_PERSISTENCE": "数据持久化插件",
    "STR_PLUG_INJECTION": "文件注入插件",
    "STR_PLUG_CONF_REPLACE": "启动配置替换插件",
    "STR_PLUG_PASSWORD": "密码插件",
    "STR_PLUG_IMAGELIST": "文件列表插件",
    "STR_PLUG_AUTO_MEMDISK": "自动Memdisk插件",
    "STR_PLUG_DUD": "Driver Update Disk插件",
    "STR_PLUG_DONATION": "捐助",

    "STR_PATH_TOO_LONG": "路径超过最大支持长度，请检查！",
    "STR_INPUT_TOO_LONG": "字符串超过最大支持长度，请检查！",
    "STR_INVALID_FILE_PATH": "文件路径不合法或不存在，请检查！",    
    "STR_INVALID_FILE_PATH1": "第1个文件路径不合法或不存在，请检查！",    
    "STR_INVALID_FILE_PATH2": "第2个文件路径不合法或不存在，请检查！",    
    "STR_INVALID_NEW_FILE_PATH": "new 文件路径不合法或不存在，请检查！",    
    "STR_INVALID_DIR_PATH": "文件夹路径不合法，请检查！",
    "STR_INVALID_NUMBER": "请输入合法的非负整数！",
    "STR_INVALID_AUTOSEL": "autosel 的值超过了列表实际长度！",
    "STR_INVALID_TIMEOUT": "请输入合法的超时秒数！",
    "STR_INVALID_PERCENT": "请输入 0-100 以内的整数！",
    "STR_INVALID_COLOR": "请输入合法的颜色！",
    "STR_SELECT": "请选择",
    "STR_SET_ALIAS_FOR_FILE": "为文件设置别名",
    "STR_SET_ALIAS_FOR_DIR": "为目录设置别名",
    "STR_SET_TIP_FOR_FILE": "为文件设置菜单提示信息",
    "STR_SET_TIP_FOR_DIR": "为目录设置菜单提示信息",
    "STR_SET_INJECTION_FOR_FILE": "[image] 为某一个文件设置注入",
    "STR_SET_INJECTION_FOR_DIR": "[parent] 为某个目录下的所有文件设置相同的注入",
    "STR_INVALID_ARCHIVE_PATH": "Archive 文件路径非法或不存在，请检查！",
    "STR_SET_PWD_FOR_FILE": "[file] 为指定文件设置密码",
    "STR_SET_PWD_FOR_DIR": "[parent] 为某个目录下的所有文件设置相同的密码",
    "STR_SET_AUTO_INSTALL_FOR_FILE": "[image] 为某个镜像文件设置自动安装脚本",
    "STR_SET_AUTO_INSTALL_FOR_DIR": "[parent] 为某个目录下的所有文件设置相同的自动安装脚本",

    "STR_SET_CLASS_BY_KEY": "[key] 通过文件名关键字设置类型",
    "STR_SET_CLASS_BY_DIR": "[dir] 为某个目录设置类型（只针对该目录本身，不包含里面的文件及子目录）",
    "STR_SET_CLASS_BY_PARENT": "[parent] 为某个目录下的所有子文件设置类型（只针对文件，不包含目录）",

    "STR_SET_IMAGE_PWD": "[file] 为某个镜像文件设置密码",
    "STR_SET_PARENT_PWD": "[parent] 为某个目录下的所有文件设置相同的密码",


    
    "STR_SET_SEARCH_ROOT": "设置搜索目录",
    "STR_SET_DEFAULT_IMAGE": "设置默认镜像文件",
    "STR_ADD_THEME": "添加主题",
    "STR_ADD_FONT": "添加字体",
    "STR_ADD_FILE_TO_LIST": "添加文件",
    "STR_DEFAULT_SELECT": " 默认选择",
    "STR_AUTO_TEMPLATE": "自动安装脚本",
    "STR_ADD_AUTO_TEMPLATE": "添加自动安装脚本",
    "STR_PERSISTENCE_DAT": "持久化数据文件",
    "STR_ADD_PERSISTENCE_DAT": "添加持久化数据文件",
    "STR_DUD_FILE": "DUD 文件",
    "STR_ADD_DUD_FILE": "添加 DUD 文件",

    "STR_DEL_LAST": "这是本条目中的最后一项，删除此项将会删除整个条目。是否继续？",
    "STR_CLOSE_TIP": "后台服务不可用，页面即将关闭！",
    "STR_SECURE_BOOT_ENABLE": "开启",
    "STR_SECURE_BOOT_DISABLE": "未开启",
    "STR_SYNTAX_ERROR_TIP": "ventoy.json 文件中存在语法错误，配置未加载!",
    "STR_INVALID_CONFIG_TIP": "ventoy.json 文件中存在错误配置，配置未加载!",
    "STR_CONFIG_SAVE_ERROR_TIP": "ventoy.json 文件写入失败，详细信息请参考 VentoyPlugson.log 文件!",


    "STR_XXX": "xxx"
};

var g_current_dir;
var g_current_os;
var g_current_language = 'cn';
var g_vtoy_cur_language = g_vtoy_cur_language_cn;
var g_vtoy_data_default_index = 6;

var g_file_with_extra = false;
var g_dir_with_extra = false;
var g_file_fuzzy_match = 0;
var g_file_modal_callback;
var g_dir_modal_callback;

function ventoy_file_submit(form, extra) {
    var filepath = $("#FilePath").val();
    var fileextra = $("#FileExtra").val();

    if (!filepath) {
        return;
    }

    if (extra) {
        if (!fileextra) {
            return;
        }
    }

    filepath = ventoy_replace_slash(filepath);

    if (!ventoy_common_check_path(filepath)) {
        Message.error(g_vtoy_cur_language.STR_INVALID_FILE_PATH);
        return;
    }

    if (g_file_fuzzy_match && filepath.indexOf("*") >= 0) {
        callVtoySync({
            method : 'check_fuzzy',
            path: filepath
        }, function(data) {        
            if (data.exist != 0) {
                if (typeof(g_file_modal_callback) === 'function') {
                    g_file_modal_callback(filepath, -1, fileextra);
                }
                $("#SetFileModal").modal('hide');
            } else {
                Message.error(g_vtoy_cur_language.STR_INVALID_FILE_PATH);
            }
       });
    } else {
        callVtoySync({
            method : 'check_path',
            dir: 0,
            path: filepath
        }, function(data) {        
            if (data.exist === 1) {
                if (typeof(g_file_modal_callback) === 'function') {
                    g_file_modal_callback(filepath, 1, fileextra);
                }
                $("#SetFileModal").modal('hide');
            } else {
                Message.error(g_vtoy_cur_language.STR_INVALID_FILE_PATH);
            }
       });
    }
}


var g_filepath_validator = $("#SetFileForm").validate({
    rules: {            
        FilePath : {
            required: true,
            utfmaxlen: true,
            noquotes: true
        },
        FileExtra : {
            required: false,
            utfmaxlen: true
        }
    },
    submitHandler: function(form) {
        ventoy_file_submit(form, g_file_with_extra);
    }
});

var g_dirpath_validator = $("#SetDirForm").validate({
    rules: {            
        DirPath : {
            required: true,
            utfmaxlen: true,
            noquotes: true
        },
        DirExtra : {
            required: false,
            utfmaxlen: true
        }
    },
    submitHandler: function(form) {
        var dirpath = $("#DirPath").val();
        var dirextra = $("#DirExtra").val();
        
        if (!dirpath) {
            return;
        }

        if (g_dir_with_extra) {
            if (!dirextra) {
                return;
            }
        }

        dirpath = ventoy_replace_slash(dirpath);

        if (dirpath.length > 0 && dirpath.charCodeAt(dirpath.length - 1) === 47) {
            dirpath = dirpath.substring(0, dirpath.length - 1);
        }

        if (!ventoy_common_check_path(dirpath)) {
            Message.error(g_vtoy_cur_language.STR_INVALID_DIR_PATH);
            return;
        }

        callVtoySync({
            method : 'check_path',
            dir: 1,
            path: dirpath                
        }, function(data) {        
            if (data.exist === 1) {
                if (typeof(g_dir_modal_callback) === 'function') {
                    g_dir_modal_callback(dirpath, dirextra);
                }
                $("#SetDirModal").modal('hide');
            } else {
                Message.error(g_vtoy_cur_language.STR_INVALID_DIR_PATH);
            }
       });
    }
});

function VtoySelectFilePath(cb, para) {
    g_file_fuzzy_match = para.fuzzy;
    
    if (para.extra) {
        $('div[id=id_div_file_extra]').show();
        $('#SetFileForm_extra').text(para.extra_title);
    } else {
        $('div[id=id_div_file_extra]').hide();
    }

    $('span[id=id_span_filepath_tip1]').each(function(){
        $(this).text(para.tip1);
    });
    $('span[id=id_span_filepath_tip2]').each(function(){
        $(this).text(para.tip2);
    });
    $('span[id=id_span_filepath_tip3]').each(function(){
        $(this).text(para.tip3);
    });

    if (g_current_language === 'en') {
        if (para.title.length === 0) {
            $('#SetFileForm #SetFileForm_lang_1').text("Set File Path");
        } else {
            $('#SetFileForm #SetFileForm_lang_1').text(para.title);
        }
        
        $('#SetFileForm #SetFileForm_lang_2').text("File Path");
        $('#SetFileForm #SetFileForm_lang_3').text(" OK");
        $('#SetFileForm #SetFileForm_lang_4').text("Cancel");
        $('#SetFileForm #id_note_setfile_cn').hide();
        $('#SetFileForm #id_note_setfile_en').show();
    } else {
        if (para.title.length === 0) {
            $('#SetFileForm #SetFileForm_lang_1').text("设置文件路径");
        } else {
            $('#SetFileForm #SetFileForm_lang_1').text(para.title);
        }
        $('#SetFileForm #SetFileForm_lang_2').text("文件路径");
        $('#SetFileForm #SetFileForm_lang_3').text("确定");
        $('#SetFileForm #SetFileForm_lang_4').text("取消");
        $('#SetFileForm #id_note_setfile_cn').show();
        $('#SetFileForm #id_note_setfile_en').hide();
    }

    if (para.tip3.length > 0) {
        if (g_current_language === 'en') {
            $('#SetFileForm #id_note_tip3_en').show();
            $('#SetFileForm #id_note_tip3_cn').hide();
        } else {
            $('#SetFileForm #id_note_tip3_cn').show();
            $('#SetFileForm #id_note_tip3_en').hide();
        }
    } else {
        $('#SetFileForm #id_note_tip3_en').hide();
        $('#SetFileForm #id_note_tip3_cn').hide();
    }

    g_file_modal_callback = cb;
    g_file_with_extra = para.extra;
    g_filepath_validator.settings.rules.FileExtra.required = g_file_with_extra;
    g_filepath_validator.resetForm();
    $("#SetFileModal").modal();            
}


function VtoySelectDirPath(cb, para) {
    $('span[id=id_span_dirpath_tip]').each(function(){
        $(this).text(para.tip);
    });
    $('span[id=id_span_dirpath_tip3]').each(function(){
        $(this).text(para.tip3);
    });

    if (para.extra) {
        $('div[id=id_div_dir_extra]').show();
        $('label[id=SetDirForm_extra]').text(para.extra_title);
    } else {
        $('div[id=id_div_dir_extra]').hide();
    }

    if (g_current_language === 'en') {
        if (para.title.length === 0) {
            $('#SetDirForm #SetDirForm_lang_1').text("Set Directory Path");
        } else {
            $('#SetDirForm #SetDirForm_lang_1').text(para.title);
        }
        $('#SetDirForm #SetDirForm_lang_2').text("Directory Path");
        $('#SetDirForm #SetDirForm_lang_3').text(" OK");
        $('#SetDirForm #SetDirForm_lang_4').text("Cancel");
        $('#SetDirForm #id_note_setfile_cn').hide();
        $('#SetDirForm #id_note_setfile_en').show();
    } else {
        if (para.title.length === 0) {
            $('#SetDirForm #SetDirForm_lang_1').text("设置文件夹路径");
        } else {
            $('#SetDirForm #SetDirForm_lang_1').text(para.title);
        }
        $('#SetDirForm #SetDirForm_lang_2').text("文件夹路径");
        $('#SetDirForm #SetDirForm_lang_3').text("确定");
        $('#SetDirForm #SetDirForm_lang_4').text("取消");
        $('#SetDirForm #id_note_setfile_cn').show();
        $('#SetDirForm #id_note_setfile_en').hide();
    }

    if (para.tip3.length > 0) {
        if (g_current_language === 'en') {
            $('#SetDirForm #id_note_tip3_en').show();
            $('#SetDirForm #id_note_tip3_cn').hide();
        } else {
            $('#SetDirForm #id_note_tip3_cn').show();
            $('#SetDirForm #id_note_tip3_en').hide();
        }
    } else {
        $('#SetDirForm #id_note_tip3_en').hide();
        $('#SetDirForm #id_note_tip3_cn').hide();
    }

    g_dir_modal_callback = cb;
    g_dir_with_extra = para.extra;
    g_dirpath_validator.settings.rules.DirExtra.required = g_dir_with_extra;
    g_dirpath_validator.resetForm(); 
    $("#SetDirModal").modal();            
}

function VtoyCommonChangeLanguage(newlang) {
    if (newlang === 'en') {
        g_vtoy_cur_language = g_vtoy_cur_language_en;
        ;$.extend($.validator.messages, {
            required: "This field is required",
            remote: "Please modify this field",    
            maxlength: $.validator.format("You can enter up to {0} characters"),
            minlength: $.validator.format("Must enter at least {0} characters"),
            rangelength: $.validator.format("Please input {0} to {1} characters"),
            range: $.validator.format("The input range is from {0} to {1}"),
            max: $.validator.format("Please input a number less than or equal to {0}"),
            min: $.validator.format("Please input a number bigger than or equal to {0}"),
            utfmaxlen: $.validator.format("The string exceeds the maximum supported length"),
            start_slash: $.validator.format("Must start with /"),
            noquotes: $.validator.format("Can not include double quotes"),
            filenamepart:$.validator.format("As part of file name, can not include invalid characters"),
            printascii: $.validator.format("Can not include non-ascii characters.")
        });

        $("a[id=id_a_official_doc]").each(function(){
            var oldlink = $(this).attr('href');
            var newlink = oldlink.replace("/cn/", "/en/");
            $(this).attr('href', newlink);
        });
        
        $("span[id=id_span_official_doc]").each(function(){
            $(this).text(" Plugin Official Document");
        });
        
        $('#id_span_language').text("中文");
        
        $("tr[id=tr_title_desc_cn]").each(function(){
            $(this).hide();
        });
        
        $("tr[id=tr_title_desc_en]").each(function(){
            $(this).show();
        });
      
        $("th[id=id_th_file_path]").each(function(){
            $(this).text("Full File Path");
        });

        $("span[id=id_span_desc_cn]").each(function(){
            $(this).hide();
        });

    } else {
        g_vtoy_cur_language = g_vtoy_cur_language_cn;
        ;$.extend($.validator.messages, {
            required: "这是必填字段",
            remote: "请修正此字段",    
            maxlength: $.validator.format("最多可以输入 {0} 个字符"),
            minlength: $.validator.format("最少要输入 {0} 个字符"),
            rangelength: $.validator.format("请输入长度在 {0} 到 {1} 之间的字符串"),
            range: $.validator.format("取值范围{0}到{1}"),
            max: $.validator.format("请输入不大于 {0} 的数值"),
            min: $.validator.format("请输入不小于 {0} 的数值"),
            utfmaxlen: $.validator.format("超过最大长度"),
            start_slash: $.validator.format("必须以反斜杠 / 开头"),
            noquotes: $.validator.format("不能包含双引号"),
            filenamepart:$.validator.format("作为文件名的一部分，不能包含特殊的符号"),
            printascii: $.validator.format("不能包含中文或其他非 ascii 字符。")
        });
        
        $("a[id=id_a_official_doc]").each(function(){
            var oldlink = $(this).attr('href');
            var newlink = oldlink.replace("/en/", "/cn/");
            $(this).attr('href', newlink);
        });
        
        $("span[id=id_span_official_doc]").each(function(){
            $(this).text(" 插件官网文档");
        });
        
        $('#id_span_language').text("English");
        
        $("tr[id=tr_title_desc_cn]").each(function(){
            $(this).show();
        });
        
        $("tr[id=tr_title_desc_en]").each(function(){
            $(this).hide();
        });
        
        $("th[id=id_th_file_path]").each(function(){
            $(this).text("文件路径");
        });

        $("span[id=id_span_desc_cn]").each(function(){
            $(this).show();
        });
    }

    $("span[id=id_span_menu_device]").text(g_vtoy_cur_language.STR_PLUG_DEVICE);
    $("span[id=id_span_menu_control]").text(g_vtoy_cur_language.STR_PLUG_CONTROL);
    $("span[id=id_span_menu_theme]").text(g_vtoy_cur_language.STR_PLUG_THEME);
    $("span[id=id_span_menu_alias]").text(g_vtoy_cur_language.STR_PLUG_ALIAS);
    $("span[id=id_span_menu_tip]").text(g_vtoy_cur_language.STR_PLUG_TIP);
    $("span[id=id_span_menu_class]").text(g_vtoy_cur_language.STR_PLUG_CLASS);
    $("span[id=id_span_menu_auto_install]").text(g_vtoy_cur_language.STR_PLUG_AUTO_INSTALL);
    $("span[id=id_span_menu_persistence]").text(g_vtoy_cur_language.STR_PLUG_PERSISTENCE);
    $("span[id=id_span_menu_injection]").text(g_vtoy_cur_language.STR_PLUG_INJECTION);
    $("span[id=id_span_menu_conf_replace]").text(g_vtoy_cur_language.STR_PLUG_CONF_REPLACE);
    $("span[id=id_span_menu_password]").text(g_vtoy_cur_language.STR_PLUG_PASSWORD);
    $("span[id=id_span_menu_imagelist]").text(g_vtoy_cur_language.STR_PLUG_IMAGELIST);
    $("span[id=id_span_menu_auto_memdisk]").text(g_vtoy_cur_language.STR_PLUG_AUTO_MEMDISK);
    $("span[id=id_span_menu_dud]").text(g_vtoy_cur_language.STR_PLUG_DUD);
    $('#id_span_save').text(g_vtoy_cur_language.STR_SAVE);
    $('#id_span_reset').text(g_vtoy_cur_language.STR_RESET);
    $('#id_span_donation').text(g_vtoy_cur_language.STR_PLUG_DONATION);

    $("span[id=id_span_btn_add]").each(function(){
        $(this).text(g_vtoy_cur_language.STR_ADD);
    });
    $("span[id=id_span_btn_del]").each(function(){
        $(this).text(g_vtoy_cur_language.STR_DEL);
    });

    $("span[id=id_span_enable]").each(function(){
        $(this).text(g_vtoy_cur_language.STR_ENABLE);
    });

    $("th[id=id_th_operation]").each(function(){
        $(this).text(g_vtoy_cur_language.STR_OPERATION);
    });
    $("th[id=id_th_status]").each(function(){
        $(this).text(g_vtoy_cur_language.STR_STATUS);
    });

    $('span [id=id_span_valid').each(function(){
        $(this).text(g_vtoy_cur_language.STR_VALID);
    });
    $('span [id=id_span_invalid').each(function(){
        $(this).text(g_vtoy_cur_language.STR_INVALID);
    });

    $("td[id=td_title_desc]").each(function(){
        $(this).text(g_vtoy_cur_language.STR_OPT_DESC);
    });
    
    $("td[id=td_title_setting]").each(function(){
        $(this).text(g_vtoy_cur_language.STR_OPT_SETTING);
    });
}

  
function ventoy_get_status_line(dir, exist) {
    if (dir) {
        if (exist === 0) {
        return '<span id="id_span_dir_nonexist" style="line-height: 1.5;" class="label pull-left bg-red">' + g_vtoy_cur_language.STR_DIR_NONEXIST + '</span>';
        } else {
        return '<span id="id_span_dir_exist" style="line-height: 1.5;" class="label pull-left bg-green">' + g_vtoy_cur_language.STR_DIR_EXIST + '</span>';
        }
    } else {
        if (exist === -1) {
        return '<span id="id_span_file_fuzzy" style="line-height: 1.5;" class="label pull-left bg-yellow">' + g_vtoy_cur_language.STR_FILE_FUZZY + '</span>';
        } else if (exist === 1) {
        return '<span id="id_span_file_exist" style="line-height: 1.5;" class="label pull-left bg-green">' + g_vtoy_cur_language.STR_FILE_EXIST + '</span>';
        } else {
        return '<span id="id_span_file_nonexist" style="line-height: 1.5;" class="label pull-left bg-red">' + g_vtoy_cur_language.STR_FILE_NONEXIST + '</span>';
        }
    }
}



var g_type_select_callback;

var g_type_select_validator = $("#TypeSelectForm").validate({    
    submitHandler: function(form) {
        var sel =  parseInt($('input:radio[name=name_select_type_radio]:checked').val());
        if (typeof(g_type_select_callback) === 'function') {
            g_type_select_callback(sel);
        }

        $("#TypeSelectModal").modal('hide');
    }
});

function VtoySelectType(cb, para) {

    $('#TypeSelectForm #TypeSelForm_lang_1').text(g_vtoy_cur_language.STR_SELECT);
    
    if (g_current_language === 'en') {
        $('#TypeSelectForm #TypeSelForm_lang_2').text(" OK");
        $('#TypeSelectForm #TypeSelForm_lang_3').text("Cancel");
    } else {
        $('#TypeSelectForm #TypeSelForm_lang_2').text("确定");
        $('#TypeSelectForm #TypeSelForm_lang_3').text("取消");
    }
    
    var $tbl = $("#id_type_select_table tbody");
    $tbl.empty();

    for (var i = 0; i < para.length; i++) {
        var $tr;

        if (para[i].selected) {
            $tr = $('<tr><td><label class="radio-inline"><input type="radio" checked="checked" name="name_select_type_radio" value="' + i + '"/>' + para[i].tip + '</label></td></tr>');
        } else {
            $tr = $('<tr><td><label class="radio-inline"><input type="radio" name="name_select_type_radio" value="' + i + '"/>' + para[i].tip + '</label></td></tr>');
        }

        $tbl.append($tr);
    }

    g_type_select_callback = cb;
    g_type_select_validator.resetForm(); 
    $("#TypeSelectModal").modal();            
}


var g_set_key_callback;

var g_set_key_validator = $("#SetKeyForm").validate({    
    rules: {            
        SetKeyKey : {
            required: true,
            utfmaxlen: true      
        },
        SetKeyValue : {
            required: true,
            utfmaxlen: true,
            filenamepart: true        
        }
    },

    submitHandler: function(form) {
        var key = $('input:text[id=SetKeyKey]').val();
        var val = $('input:text[id=SetKeyValue]').val();

        if ((!key) || (!val))
        {
            return;
        }

        if (typeof(g_set_key_callback) === 'function') {
            g_set_key_callback(key, val);
        }

        $("#SetKeyModal").modal('hide');
    }
});

function VtoySetKey(cb, para) {

    $('#SetKeyForm #SetKeyForm_lang_1').text(para.title);
    $('#SetKeyForm #SetKeyForm_lang_2').text(para.title1);
    $('#SetKeyForm #SetKeyForm_lang_3').text(para.title2);

    if (g_current_language === 'en') {
        $('#SetKeyForm #SetKeyForm_lang_4').text(" OK");
        $('#SetKeyForm #SetKeyForm_lang_5').text("Cancel");
    } else {
        $('#SetKeyForm #SetKeyForm_lang_4').text("确定");
        $('#SetKeyForm #SetKeyForm_lang_5').text("取消");
    }
    
    g_set_key_callback = cb;
    g_set_key_validator.resetForm(); 
    $("#SetKeyModal").modal();            
}

var g_valid_color_name = [
    "black",
    "blue",
    "green",
    "cyan",
    "red",
    "magenta",
    "brown",
    "light-gray",
    "dark-gray",
    "light-blue",
    "light-green",
    "light-cyan",
    "light-red",
    "light-magenta",
    "yellow",
    "white"
];

function ventoy_check_color(color) {
    if (/^#[0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]$/.test(color)) {
        return true;
    } else {
        for (var i = 0; i < g_valid_color_name.length; i++) {
            if (g_valid_color_name[i] === color) {
                return true;
            }
        }
    }

    return false;
}

function ventoy_check_percent(percent) {
    if (percent.length > 0) {
        return true;
    } else {
        return false;
    }
}


function ventoy_check_file_path(isopath, fuzzy, cb) {
    if (fuzzy && isopath.indexOf("*") >= 0) {
        callVtoySync({
            method : 'check_fuzzy',
            path: isopath
        }, function(data) {        
            if (data.exist != 0) {
                if (typeof(cb) === 'function') {
                    cb(data.exist);
                }
            } else {
                Message.error(g_vtoy_cur_language.STR_INVALID_FILE_PATH);
            }
        });
    } else {
        callVtoySync({
            method : 'check_path',
            dir: 0,
            path: isopath
        }, function(data) {        
            if (data.exist === 1) {
                if (typeof(cb) === 'function') {
                    cb(data.exist);
                }
            } else {
                Message.error(g_vtoy_cur_language.STR_INVALID_FILE_PATH);
            }
        });
    }
}

function ventoy_random_string(e) {  
    var t = "abcdefhijkmnprstwxyz2345678";
    var a = t.length;
    var n = "";

    e = e || 4;
    for (i = 0; i < e; i++) n += t.charAt(Math.floor(Math.random() * a));
    return n
}


var g_set_filefile_callback;

var g_set_filefile_validator = $("#SetFileFileForm").validate({    
    rules: {            
        FileFilePath1 : {
            required: true,
            utfmaxlen: true
        },
        FileFilePath2 : {
            required: true,
            utfmaxlen: true          
        }
    },

    submitHandler: function(form) {
        var path1 = $('input:text[id=FileFilePath1]').val();
        var path2 = $('input:text[id=FileFilePath2]').val();

        if ((!path1) || (!path2))
        {
            return;
        }

        path1 = ventoy_replace_slash(path1);

        if (!ventoy_common_check_path(path1)) {
            Message.error(g_vtoy_cur_language.STR_INVALID_FILE_PATH1);
            return;
        }

        path2 = ventoy_replace_slash(path2);

        if (!ventoy_common_check_path(path2)) {
            Message.error(g_vtoy_cur_language.STR_INVALID_FILE_PATH2);
            return;
        }

        callVtoy({
            method : 'check_path2',
            dir1: 0,
            fuzzy1: 1,
            path1: path1,
            dir2: 0,
            fuzzy2: 0,
            path2: path2
        }, function(retdata) {
            if (retdata.exist1 != 0 && retdata.exist2 != 0) {
                if (typeof(g_set_filefile_callback) === 'function') {
                    g_set_filefile_callback(retdata.exist1, path1, path2);
                }

                $("#SetFileFileModal").modal('hide');
            } else if (retdata.exist1 === 0) {
              Message.error(g_vtoy_cur_language.STR_INVALID_FILE_PATH1);
            } else {
              Message.error(g_vtoy_cur_language.STR_INVALID_FILE_PATH2);
            }
        });
    }
});

function VtoySetFileFile(cb, para) {

    $('#SetFileFileForm #SetFileFileForm_title').text(para.title);
    $('#SetFileFileForm #SetFileFileForm_label1').text(para.label1);
    $('#SetFileFileForm #SetFileFileForm_label2').text(para.label2);

    if (g_current_language === 'en') {
        $('#SetFileFileForm #SetFileFileForm_ok').text(" OK");
        $('#SetFileFileForm #SetFileFileForm_cancel').text("Cancel");

        $('#SetFileFileForm #id_note_filefile_cn').hide();
        $('#SetFileFileForm #id_note_filefile_en').show();

    } else {
        $('#SetFileFileForm #SetFileFileForm_ok').text("确定");
        $('#SetFileFileForm #SetFileFileForm_cancel').text("取消");

        $('#SetFileFileForm #id_note_filefile_en').hide();
        $('#SetFileFileForm #id_note_filefile_cn').show();
    }

    $('span[id=id_span_filefile_tip1]').each(function(){
        $(this).text(para.tip1);
    });
    $('span[id=id_span_filefile_tip2]').each(function(){
        $(this).text(para.tip2);
    });
    $('span[id=id_span_filefile_tip3]').each(function(){
        $(this).text(para.tip3);
    });
    
    g_set_filefile_callback = cb;
    g_set_filefile_validator.resetForm(); 
    $("#SetFileFileModal").modal();
}


var g_set_dirfile_callback;

var g_set_dirfile_validator = $("#SetDirFileForm").validate({    
    rules: {            
        DirFilePath1 : {
            required: true,
            utfmaxlen: true
        },
        DirFilePath2 : {
            required: true,
            utfmaxlen: true          
        }
    },

    submitHandler: function(form) {
        var path1 = $('input:text[id=DirFilePath1]').val();
        var path2 = $('input:text[id=DirFilePath2]').val();

        if ((!path1) || (!path2))
        {
            return;
        }

        path1 = ventoy_replace_slash(path1);

        if (!ventoy_common_check_path(path1)) {
            Message.error(g_vtoy_cur_language.STR_INVALID_FILE_PATH1);
            return;
        }

        path2 = ventoy_replace_slash(path2);

        if (!ventoy_common_check_path(path2)) {
            Message.error(g_vtoy_cur_language.STR_INVALID_FILE_PATH2);
            return;
        }

        callVtoy({
            method : 'check_path2',
            dir1: 1,
            fuzzy1: 0,
            path1: path1,
            dir2: 0,
            fuzzy2: 0,
            path2: path2
        }, function(retdata) {
            if (retdata.exist1 != 0 && retdata.exist2 != 0) {
                if (typeof(g_set_dirfile_callback) === 'function') {
                    g_set_dirfile_callback(path1, path2);
                }

                $("#SetDirFileModal").modal('hide');
            } else if (retdata.exist1 === 0) {
              Message.error(g_vtoy_cur_language.STR_INVALID_DIR_PATH);
            } else {
              Message.error(g_vtoy_cur_language.STR_INVALID_FILE_PATH2);
            }
        });
    }
});

function VtoySetDirFile(cb, para) {

    $('#SetDirFileModal #SetDirFileForm_title').text(para.title);
    $('#SetDirFileModal #SetDirFileForm_label1').text(para.label1);
    $('#SetDirFileModal #SetDirFileForm_label2').text(para.label2);

    if (g_current_language === 'en') {
        $('#SetDirFileModal #SetDirFileForm_ok').text(" OK");
        $('#SetDirFileModal #SetDirFileForm_cancel').text("Cancel");

        $('#SetDirFileModal #id_note_dirfile_cn').hide();
        $('#SetDirFileModal #id_note_dirfile_en').show();

    } else {
        $('#SetDirFileModal #SetDirFileForm_ok').text("确定");
        $('#SetDirFileModal #SetDirFileForm_cancel').text("取消");

        $('#SetDirFileModal #id_note_dirfile_en').hide();
        $('#SetDirFileModal #id_note_dirfile_cn').show();
    }

    $('span[id=id_span_dirfile_tip1]').each(function(){
        $(this).text(para.tip1);
    });
    $('span[id=id_span_dirfile_tip2]').each(function(){
        $(this).text(para.tip2);
    });
    
    g_set_dirfile_callback = cb;
    g_set_dirfile_validator.resetForm(); 
    $("#SetDirFileModal").modal();
}

function ventoy_get_xslg_addbtn(mclass) {
    return '<button class="btn btn-xs btn-lg btn-success btn-add ' + mclass + '"><span class="fa fa-plus">&nbsp;&nbsp;</span><span id="id_span_btn_add">'+g_vtoy_cur_language.STR_ADD+'</span></button>';
}

function ventoy_get_xslg_delbtn(mclass) {
    return '<button class="btn btn-xs btn-lg btn-danger btn-del '+mclass+'"><span class="fa fa-trash">&nbsp;&nbsp;</span><span id="id_span_btn_del">'+g_vtoy_cur_language.STR_DEL+'</span></button>';
}

function ventoy_get_addbtn(mclass) {
    return '<button class="btn btn-success btn-add ' + mclass + '"><span class="fa fa-plus">&nbsp;&nbsp;</span><span id="id_span_btn_add">'+g_vtoy_cur_language.STR_ADD+'</span></button>';
}

function ventoy_get_delbtn(mclass) {
    return '<button class="btn btn-danger btn-del '+mclass+'"><span class="fa fa-trash">&nbsp;&nbsp;</span><span id="id_span_btn_del">'+g_vtoy_cur_language.STR_DEL+'</span></button>';
}

function ventoy_confirm(title, cb, data1, data2) {
    Modal.confirm({msg:g_vtoy_cur_language.STR_DEL_LAST}).on(function(e) {
        if (e) {
            if (typeof(cb) === 'function') {
                cb(data1, data2);
            }
        }
    });
}
