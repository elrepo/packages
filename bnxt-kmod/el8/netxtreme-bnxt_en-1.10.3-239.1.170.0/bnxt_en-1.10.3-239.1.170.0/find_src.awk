#!/usr/bin/awk -f

BEGIN{
    if (struct) {
	start="struct " struct " {"
    } else if (enum) {
	start="enum " enum " {"
    } else if (define) {
	pattern="#define " define
	open=1
    } else {
	print "Usage: find_src.awk -v <struct | enum | define>=<regex> [-v pattern=<regex>]"
	print "\nPrints lines associated with matched elements and optionally further constrains matching within such elements by an additional regex pattern."
	exit 1
    }
}
$0~/{/{
    open && open++
}
$0~start{
    open=1;
}
{
    if (line_cont) {
	print $0
	line_cont=match($0, /\\$/)
    }
}
$0~pattern{
    if (open) {
	print $0
	line_cont=match($0, /\\$/)
    }
}
$0~/}/{
    open && open--
}
