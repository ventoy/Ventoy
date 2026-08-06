
// 包装ajax请求
function callVtoy(p1, p2, p3) {
    const url = '/vtoy/json';
    const data = {};
    const func = function(data) {};

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
                Message.error(vtoy_cur_language.STR_WEB_REMOTE_ABNORMAL);
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
                        Message.error(vtoy_cur_language.STR_WEB_REQUEST_TIMEOUT);
                        break;
                    }
                    case 'Service Unavailable':
                    {
                        Message.error(vtoy_cur_language.STR_WEB_SERVICE_UNAVAILABLE);
                        break;
                    }
                    case 'abort':
                    {
                        break;
                    }
                    default:
                    {
                        Message.error(vtoy_cur_language.STR_WEB_COMMUNICATION_ERR + errorThrown);
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
            if (data.result === 'tokenerror') {
                const titlestr = '<span class="fa  fa-minus-circle" style="color:#dd4b39; font-weight:bold;"> ' + vtoy_cur_language.STR_ERROR + '</span>';
                const msgstr = '<span style="font-size:14px; font-weight:bold;"> ' + vtoy_cur_language.STR_WEB_TOKEN_MISMATCH + '</span>';
                
                Modal.alert({title:titlestr, msg:msgstr, btnok:vtoy_cur_language.STR_BTN_OK }).on(function(e) {
                    window.location.reload(true);
                });
            } 
            else if (data.result === 'busy') {
                const titlestr = '<span class="fa fa-check-circle" style="color:green; font-weight:bold;"> ' + vtoy_cur_language.STR_INFO + '</span>';
                const msgstr = '<span style="font-size:14px; font-weight:bold;"> ' + vtoy_cur_language.STR_WEB_SERVICE_BUSY + '</span>';
                Modal.alert({title:titlestr, msg:msgstr, btnok:vtoy_cur_language.STR_BTN_OK });                
            }else {
                func(data);
            }
        },
        error: function(xmlHttpRequest, textStatus, errorThrown) {
            if(undefined === errorThrown)
            {
                Message.error(vtoy_cur_language.STR_WEB_REMOTE_ABNORMAL);
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
                        Message.error(vtoy_cur_language.STR_WEB_REQUEST_TIMEOUT);
                        break;
                    }
                    case 'Service Unavailable':
                    {
                        Message.error(vtoy_cur_language.STR_WEB_SERVICE_UNAVAILABLE);
                        break;
                    }
                    case 'abort':
                    {
                        break;
                    }
                    default:
                    {
                        Message.error(vtoy_cur_language.STR_WEB_COMMUNICATION_ERR + errorThrown);
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

const vtoy = {
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
    const _showMsg = function(type, msg, time) {
        const o = {type : type, msg : msg };
        if(time) {
            o.time = time;
        }
        _show(o);
    }
    
    const _show = function(options) {
        const ops = {
            msg : "提示内容",
            type: 'S',
            time: 3000
        };
        $.extend(ops, options);

        const msg_class = 'alert-success';
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
        const $messageContainer = $("#fcss_message");
        if($messageContainer.length === 0) {
            $messageContainer = $('<div id="fcss_message" style="position:fixed; left: 20%; right: 20%; top:0px; z-index:99999999"></div>');
            $messageContainer.appendTo($('body'));
        }
        const $div = $('<div class="alert ' + msg_class + ' alert-dismissible fade in" role="alert" style="margin-bottom: 0; padding-top:10px; padding-bottom: 10px;"></div>');
        const $btn = $('<button type="button" class="close" data-dismiss="alert" aria-label="Close"><span aria-hidden="true">×</span></button>');
        $div.append($btn).append(ops.msg).appendTo($messageContainer);
        setTimeout(function() {
            $div.remove();
        }, ops.time);
    }
    
    const _success = function(msg, time) {
        _showMsg('s', msg, time);
    }
    const _error = function(msg, time) {
        _showMsg('e', msg, time || 5000);
    }
    const _warn = function(msg, time) {
        _showMsg('w', msg, time);
    }
    const _info = function(msg, time) {
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

