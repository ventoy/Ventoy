# ColVis

ColVis adds a button to the toolbars around DataTables which gives the end user of the table the ability to dynamically change the visibility of the columns in the table:

* Dynamically show and hide columns in a table
* Very easy integration with DataTables
* Ability to exclude columns from being either hidden or shown
* Save saving integration with DataTables


# Installation

To use ColVis, first download DataTables ( http://datatables.net/download ) and place the unzipped ColVis package into a `extensions` directory in the DataTables package. This will allow the pages in the examples to operate correctly. To see the examples running, open the `examples` directory in your web-browser.


# Basic usage

ColVis is initialised using the `C` option that it adds to DataTables' `dom` option. For example:

```js
$(document).ready( function () {
    $('#example').dataTable( {
        "dom": 'C<"clear">lfrtip'
    } );
} );
```


# Documentation / support

* Documentation: http://datatables.net/extensions/colvis/
* DataTables support forums: http://datatables.net/forums


# GitHub

If you fancy getting involved with the development of ColVis and help make it better, please refer to its GitHub repo: https://github.com/DataTables/ColVis

