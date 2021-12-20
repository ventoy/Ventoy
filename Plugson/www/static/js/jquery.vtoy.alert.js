
/**
 * 用于bootstap框架下提示信息框
 * demo: 
 * 
 * 模态框
 * Modal.confirm({msg: "是否确定提交？"}).on( function (e) {alert("返回结果：" + e);});
 * Modal.alert({msg:"该记录已删除！"})
 * Modal.process('show'/'hide') 隐藏或显示全屏、进度条
 * 
 * 非模态框
 * Message.show({ type : 'S|W|E|I', msg: '提示信息' })
 * Message.success('成功信息')
 * Message.error('错误信息')
 * Message.warn('警告信息')
 * Message.info('提示信息')
 * Message.warn('警告信息',10000) //10000为显示时长
 */
;$(function() {
    
	window.Modal = function() {
		var reg = new RegExp("\\[([^\\[\\]]*?)\\]", 'igm');
		var alr = $("#msgAlertDiv");
        
        if (alr.length == 0) {
            alr = $('<div id="msgAlertDiv" class="modal fade"></div>')
            $("body").append(alr);
        }
    
		var ahtml = ' <div class="modal-dialog">'
            + '<div class="modal-content">'
            + '<div class="modal-header">'
            + '<h3 class="modal-title"><B style="font-weight: 400;"> [Title]</B></h3>'
            + '</div>'
            + '<div class="modal-body " style="word-break: break-all;display: block;">'
            + '[Message]'
            + '</div>'
            + '<div class="modal-footer" >'
            + '<button type="button" class="btn btn-success ok" data-dismiss="modal">[BtnOk]</button>'
            + '[UserDefined]'
            + '<button type="button" class="btn cancel" data-dismiss="modal">[BtnCancel]</button>'
            + '</div>' + '</div>' + '</div>';

		var _alert = function(options) {
			alr.html(ahtml); // 复原
			alr.find('.ok').removeClass('btn-success').addClass('btn-primary');
            
            if (document.body.clientHeight > 400) {
				alr.find('.modal-dialog').css("top",((document.body.clientHeight - 400) / 2)); 
			}
            
			alr.find('.cancel').hide();
			_dialog(options);

			return {
				on : function(callback) {
					if (callback && callback instanceof Function) {
						alr.find('.ok').click(function() {
							callback(true)
						});
					}
				}
			};
		};

		var _confirm = function(options) {
			alr.html(ahtml);
			alr.find('.ok').removeClass('btn-primary').addClass('btn-success');
			alr.find('.cancel').show();
            if (document.body.clientHeight > 400) {
				alr.find('.modal-dialog').css("top",((document.body.clientHeight - 400) / 2)); 
			}
            
			_dialog(options);

			return {
				on : function(callback) {
					if (callback && callback instanceof Function) {
						alr.find('.ok').click(function() {
							callback(true);
						});
						alr.find('.cancel').click(function() {
							callback(false);
						});
						alr.find('.userDefiend').click(function() {
                            callback("userDefiend");
                        });
					}
				}
			};
		};

		var _dialog = function(options) {
			var ops = {
				msg : '',
				title : g_vtoy_cur_language.STR_INFO,
				btnok : g_vtoy_cur_language.STR_BTN_OK,
				btncl : g_vtoy_cur_language.STR_BTN_CANCEL,
				userDefined : ""
			};

			$.extend(ops, options);

			var html = alr.html().replace(reg, function(node, key) {
				return {
					Title : ops.title,
					Message : ops.msg,
					BtnOk : ops.btnok,
					BtnCancel : ops.btncl,
					UserDefined : ops.userDefined
				}[key];
			});

			alr.html(html);
			alr.modal({
				width : 500,
				backdrop : 'static'
			});
		}

		var _process = function(showOrHide,time) {
		    var defaultTime = 100;
		    if($.isNumeric(time)) {
		        defaultTime = time;
		    }
		    var $proc;
            $proc = $("#vtoy_proc");
            if('hide' === showOrHide) {
                $proc.remove();
            } else if ('show' === showOrHide) {
                if($proc.length == 1) {
                    return;
                }
                $(document).find(":focus").blur();
                $proc = $('<div id="vtoy_proc" class="loading"></div>');
                $("body").append($proc);
                setTimeout(function() {
                    $proc.replaceWith('<div id="vtoy_proc" class="loading" style="background-color: rgba(0, 0, 0, 0.2);"><div class="rectbox"><div class="title">DATA</div><div class="rect rect1"></div><div class="rect rect2"></div><div class="rect rect3"></div><div class="rect rect4"></div><div class="rect rect5"></div></div></div>');
                }, $.isNumeric(time) ? time : 100);
            } else {
                alert("Modal.process参数必须为show/hide");
            }
		}
		
		return {
			alert : _alert,
			confirm : _confirm,
			process : _process
		}

	}();
	
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
		    _showMsg('e', msg, time || 6000);
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
	
});