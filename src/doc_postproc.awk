#!/usr/bin/awk -f

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

/<div class="Nd">/ {
	# Make the description blurb inline, as with terminal output
	gsub(/div/, "span")
}

BEGIN {
	pages["gbz80",  7] = 1
	pages["rgbds",  5] = 1
	pages["rgbds",  7] = 1
	pages["rgbasm", 1] = 1
	pages["rgbasm", 5] = 1
	pages["rgblink",1] = 1
	pages["rgblink",5] = 1
	pages["rgbfix", 1] = 1
	pages["rgbgfx", 1] = 1
}
/<a class="Xr">/ {
	# Link to other pages in the doc
	for (i in pages) {
		split(i, page, SUBSEP)
		name = page[1]
		section = page[2]
		gsub(sprintf("<a class=\"Xr\">%s\\(%d\\)", name, section),
		     sprintf("<a class=\"Xr\" href=\"%s.%d.html\">%s(%d)", name, section, name, section))
	}
}

{
	# Make long opts (defined using `Fl Fl`) into a single tag
	gsub(/<code class="Fl">-<\/code><code class="Fl">/, "<code class=\"Fl\">-")
}

{
	print
}
