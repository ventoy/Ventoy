# FixedColumns

When making use of DataTables' x-axis scrolling feature (`scrollX`), you may wish to fix the left or right most columns in place. This plug-in for DataTables provides exactly this option (for non-scrolling tables, please use the FixedHeader plug-in, which can fix headers, footers and columns). Key features include:

* Freezes the left most column to the side of the table
* Option to freeze two or more columns
* Full integration with DataTables' scrolling options


# Installation

To use FixedColumns, first download DataTables ( http://datatables.net/download ) and place the unzipped FixedColumns package into a `extensions` directory in the DataTables package. This will allow the pages in the examples to operate correctly. To see the examples running, open the `examples` directory in your web-browser.


# Basic usage

FixedColumns is initialised using the `$.fn.dataTable.FixedColumns()` constructor. For example:

```js
$(document).ready(function() {
	var table = $('#example').DataTable( {
		scrollY:        "300px",
		scrollX:        true,
		scrollCollapse: true,
		paging:         false
	} );

	new $.fn.dataTable.FixedColumns( table );
} );
```


# Documentation / support

* Documentation: http://datatables.net/extensions/FixedColumns/
* DataTables support forums: http://datatables.net/forums


# GitHub

If you fancy getting involved with the development of FixedColumns and help make it better, please refer to its GitHub repo: https://github.com/DataTables/FixedColumns

