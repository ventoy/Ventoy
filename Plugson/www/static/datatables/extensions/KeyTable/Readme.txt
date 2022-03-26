# KeyTable

KeyTable provides enhanced accessibility and navigation options for DataTables enhanced tables, by allowing Excel like cell navigation on any table. Events (focus, blur, action etc) can be assigned to individual cells, columns, rows or all cells to allow advanced interaction options.. Key features include:

* Easy to use spreadsheet like interaction
* Fully integrated with DataTables
* Wide range of supported events


# Installation

To use KeyTable, first download DataTables ( http://datatables.net/download ) and place the unzipped KeyTable package into a `extensions` directory in the DataTables package. This will allow the pages in the examples to operate correctly. To see the examples running, open the `examples` directory in your web-browser.


# Basic usage

KeyTable is initialised using the `C` option that it adds to DataTables' `dom` option. For example:

```js
$(document).ready( function () {
	var table = $('#example').DataTable();
	new $.fn.dataTable.KeyTable( table );
} );
```


# Documentation / support

* Documentation: http://datatables.net/extensions/keytable/
* DataTables support forums: http://datatables.net/forums


# GitHub

If you fancy getting involved with the development of KeyTable and help make it better, please refer to its GitHub repo: https://github.com/DataTables/KeyTable

