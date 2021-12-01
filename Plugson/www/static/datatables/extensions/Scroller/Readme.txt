# Scroller

Scroller is a virtual rendering plug-in for DataTables which allows large datasets to be drawn on screen every quickly. What the virtual rendering means is that only the visible portion of the table (and a bit to either side to make the scrolling smooth) is drawn, while the scrolling container gives the visual impression that the whole table is visible. This is done by making use of the pagination abilities of DataTables and moving the table around in the scrolling container DataTables adds to the page. The scrolling container is forced to the height it would be for the full table display using an extra element.

Key features include:

* Speed! The aim of Scroller for DataTables is to make rendering large data sets fast
* Full compatibility with DataTables' deferred rendering for maximum speed
* Integration with state saving in DataTables (scrolling position is saved)
* Support for scrolling with millions of rows
* Easy to use


# Installation

To use Scroller, first download DataTables ( http://datatables.net/download ) and place the unzipped Scroller package into a `extensions` directory in the DataTables package. This will allow the pages in the examples to operate correctly. To see the examples running, open the `examples` directory in your web-browser.


# Basic usage

Scroller is initialised by simply including the letter `dt-string S` in the `dt-init dom` for the table you want to have this feature enabled on. Note that the `dt-string S` must come after the `dt-string t` parameter in `dom`. For example:

```js
$(document).ready( function () {
	$('#example').DataTable( {
		dom: 'lfrtipS'
	} );
} );
```

Note that rows in the table must all be the same height. Information in a cell which expands on to multiple lines will cause some odd behaviour in the scrolling. Additionally, the table's `cellspacing` parameter must be set to 0, again to ensure the information display is correct.


# Documentation / support

* Documentation: http://datatables.net/extensions/scroller/
* DataTables support forums: http://datatables.net/forums


# GitHub

If you fancy getting involved with the development of Scroller and help make it better, please refer to its GitHub repo: https://github.com/DataTables/Scroller

