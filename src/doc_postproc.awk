#!/usr/bin/awk -f

/^\s+<td><b class="Sy">.+<\/b><\/td>$/ {
	# Assuming that all cells whose contents are bold are heading cells,
	# use the HTML tag for those
	sub(/td><b class="Sy"/, "th");
	sub(/b><\/td/, "th");
}

BEGIN {
	in_synopsis = 0
}
/<table class="Nm">/ {
	in_synopsis = 1
}
/<\/table>/ {
	# Resets synopsis state even when already reset, but whatever
	in_synopsis = 0
}
/<code class="Fl">-[a-zA-Z]/ {
	# Add links to arg descr in synopsis section
	if (in_synopsis) {
		while (match($0, /<code class="Fl">-[a-zA-Z]+/)) {
			#         123456789012345678 -> 18 chars
			optchars = substr($0, RSTART + 18, RLENGTH - 18)
			i = length(optchars)
			while (i) {
				end = RSTART + 18 + i
				i -= 1
				len = i ? 1 : 2
				$0 = sprintf("%s<a href=\"#%s\">%s</a>%s",
				             substr($0, 0, end - len - 1),
				             substr($0, end - 1, 1),
				             substr($0, end - len, len),
				             substr($0, end))
			}
		}
	}
}

{
	# Make long opts (defined using `Fl Fl`) into a single tag
	gsub(/<code class="Fl">-<\/code>\s*<code class="Fl">/, "<code class=\"Fl\">-")
}

{
	print
}
