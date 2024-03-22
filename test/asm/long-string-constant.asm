
println """Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium dol-
oremque laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore verita-
tis et quasi architecto beatae vitae dicta sunt explicabo. Nemo enim ipsam vol-
uptatem quia voluptas sit aspernatur aut odit aut fugit, sed quia consequuntur
magni dolores eos qui ratione voluptatem sequi nesciunt. Neque porro quisquam
est, qui dolorem ipsum quia dolor sit amet, consectetur, adipisci velit, sed
quia non numquam eius modi tempora incidunt ut labore et dolore magnam aliquam
quaerat voluptatem. Ut enim ad minima veniam, quis nostrum exercitationem ullam
corporis suscipit laboriosam, nisi ut aliquid ex ea commodi consequatur? Quis
autem vel eum iure reprehenderit qui in ea voluptate velit esse quam nihil mole-
stiae consequatur, vel illum qui dolorem eum fugiat quo voluptas nulla pariatur?"""

MACRO mac
	println "\1" ; x1
	println "\1\1\1\1\1\1\1" ; x7
ENDM

	mac Hello! ; 6x7 = 42
	mac This sentence spans forty-three characters. ; 43x7 = 301
