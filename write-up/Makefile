
file_prefix = write-up
pdf_out = $(file_prefix).pdf

all : $(pdf_out)

$(pdf_out) : $(file_prefix).tex *.tex 
	latexmk -pdf $(file_prefix).tex
	rm -f $(file_prefix).aux
	rm -f $(file_prefix).log
	rm -f $(file_prefix).fdb_latexmk
	rm -f $(file_prefix).bbl
	rm -f $(file_prefix).blg
	rm -f $(file_prefix).out
	rm -f $(file_prefix).fls

preview : $(pdf_out)
	evince $(pdf_out)

clean:
	rm -f $(file_prefix).aux
	rm -f $(file_prefix).log
	rm -f $(file_prefix).fdb_latexmk
	rm -f $(file_prefix).bbl
	rm -f $(file_prefix).blg
	rm -f $(file_prefix).out
	rm -f $(file_prefix).pdf
	rm -f $(file_prefix).fls
