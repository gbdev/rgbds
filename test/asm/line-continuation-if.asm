MACRO m
ENDM

IF 0
	m
ENDC

IF 0
	m \
ENDC

IF 1
	m
ELSE
	m
ENDC

IF 1
	m
ELSE
	m \
ENDC

IF 1
	m
ELIF 0
	m
ENDC

IF 1
	m
ELIF 0
	m \
ENDC
