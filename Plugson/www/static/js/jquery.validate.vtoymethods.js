function ventoy_check_file_name_char(path) {
    for (var i = 0; i < path.length; i++) {
        var cc = path[i];
        if (cc === '/' || cc === '\\' || cc === '*' || cc === '?' || cc === '"' || cc === '<' || cc === '>' || cc === '|')
        {
            return false;
        }
    }

    return true;
}


(function(factory) {
    if (typeof define === "function" && define.amd) {
        define(["jquery", "../jquery.validate"], factory);
    } else {
        factory(jQuery);
    }
} (function($) {

    // 设置validator插件默认校验格式
    $.validator.setDefaults({
        highlight: function(element) {
            $(element).closest('.form-group').addClass('has-error');
        },
        success: function(label) {
            label.closest('.form-group').removeClass('has-error');
            label.remove();
        }
    });


    //密码
    $.validator.addMethod('password', function(value, element, params) {
        if (this.optional(element)) {
            return true;
        }
        var re = /^[^\u4e00-\u9fa5]{1,64}$/;
        return re.test(value);
    }, '密码不合法');

    $.validator.addMethod('utfmaxlen', function(value, element, params) {
        if (this.optional(element)) {
            return true;
        }

        if (ventoy_get_ulen(value) > 250) {
            return false;
        }

        return true;
    }, 'Input too long');

    $.validator.addMethod('start_slash', function(value, element, params) {
        if (this.optional(element)) {
            return true;
        }

        if (value.length > 0 && value.charCodeAt(0) != 47) {
            return false;
        }

        return true;
    }, 'Must start with /');

    $.validator.addMethod('noquotes', function(value, element, params) {
        if (this.optional(element)) {
            return true;
        }

        if (value.length > 0 && value.indexOf('"') >= 0) {
            return false;
        }

        return true;
    }, 'Can not contain double quotes');

    $.validator.addMethod('filenamepart', function(value, element, params) {
        if (this.optional(element)) {
            return true;
        }

        return ventoy_check_file_name_char(value);
    }, 'Invalid characters');
    

    $.validator.addMethod('printascii', function(value, element, params) {
        if (this.optional(element)) {
            return true;
        }

        for (var i = 0; i < value.length; i++) {
            if (value.charCodeAt(i) > 127) {
                return false;
            }
        }

        if (value.length > 0 && value.indexOf('"') >= 0) {
            return false;
        }

        return true;
    }, 'Can only use printable ascii code');
    
}));
