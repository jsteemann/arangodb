all: build-books

.PHONY:

newVersionNumber = $(shell cat ../../VERSION)

################################################################################
# per book targets
check-summary:
	@find ppbooks/$(NAME) -name \*.md |sed -e "s;ppbooks/$(NAME)/;;" |grep -vf SummaryBlacklist.txt |sort > /tmp/is_md.txt
	@cat $(NAME)/SUMMARY.md |grep '(' |sed -e "s;.*(;;" -e "s;).*;;" |sort |grep -v '# Summary' > /tmp/is_summary.txt
	@if test "`comm -3 /tmp/is_md.txt /tmp/is_summary.txt|wc -l`" -ne 0; then \
		echo "not all files are mapped to the summary!"; \
		echo " files found       |    files in summary"; \
		comm -3 /tmp/is_md.txt /tmp/is_summary.txt; \
		exit 1; \
	fi

book-check-leftover-docublocks:
	@if test "`grep -r \"@startDocuBlock\" --include \"*.md\" ppbooks/$(NAME) | wc -l`" -ne 0; then \
	  echo ; \
	  echo "@startDocuBlock markers still found in generated output files:"; \
	  grep -rl "@startDocuBlock" --include "*.md" ppbooks/$(NAME) | sed -e "s/^/- /g"; \
	  exit 1; \
	fi

book-check-restheader-leftovers:
	@if test "`find ppbooks/$(NAME) -name '*.md' -exec grep -- '^@[A-Z]*' {} \; -print | wc -l`" -gt 0; then \
		echo "found these unconverted Swagger Restapi tags: "; \
		find ppbooks/$(NAME) -name '*.md' -exec grep '^@[A-Z]*' {} \; -print; \
		exit 1; \
	fi

# Check for all lines starting with exclamation marks, except image links which are exclamation mar + bracket
book-check-mdpp-leftovers:
	@if test "`find ppbooks/$(NAME) -name '*.md' -exec grep -- '^![^\[]' {} \; -print | wc -l`" -gt 0; then \
		echo "found these unconverted Markdown-PP tags: "; \
		find ppbooks/$(NAME) -name '*.md' -exec grep '^![^\[]' {} \; -print; \
		exit 1; \
	fi


book-check-markdown-leftovers:
	@if test "`find books/$(NAME) -name '*.html' -exec grep -- '##' {} \; -print | wc -l`" -gt 0; then \
		echo "found these unconverted markdown titles: "; \
		find books/$(NAME) -name '*.html' -exec grep '##' {} \; -print; \
		exit 1; \
	fi
	@if test "`find books/$(NAME) -name '*.html' -exec grep '\.md\"' {} \; -print | wc -l`" -gt 0; then \
		echo "found dangling markdown links: "; \
		find books/$(NAME) -name '*.html' -exec grep '\.md"' {} \; -print; \
		exit 1; \
	fi

build-book-symlinks:
	echo "generate backwards compatibility symlinks:"
	cd books/$(NAME); pwd; \
	find . -name "README.mdpp" |\
	sed -e 's:README\.mdpp$$::' |\
	awk '{print "ln -s index.html " $$1 "README.html"}' |\
	bash

build-book:
	test -d ppbooks/$(NAME) || mkdir -p ppbooks/$(NAME)
	WD=`pwd`; \
	for dir in `find $(NAME) -type d `; do \
		cd $${WD}/ppbooks; \
		test -d $${dir} || mkdir -p $${dir}; \
	done
	cd ppbooks/$(NAME); test -L SUMMARY.md || ln -s ../../$(NAME)/SUMMARY.md .
	cd ppbooks/$(NAME); test -L book.json || ln -s ../../$(NAME)/book.json .
	cd ppbooks/$(NAME); cp -a ../../$(NAME)/styles/* styles/
	WD=`pwd`; \
	for pic in `find $(NAME) -name \*.png`; do \
		cd $${WD}/ppbooks; \
		echo cd $${WD}/ppbooks; \
		echo cp $${WD}/$${pic} $${pic}; \
		cp $${WD}/$${pic} $${pic}; \
	done
	python generateMdFiles.py $(NAME) ppbooks/ ../../js/apps/system/_admin/aardvark/APP/api-docs.json

	cd ppbooks/$(NAME) && sed -i -e 's/VERSION_NUMBER/v$(newVersionNumber)/g' styles/header.js
	cd ppbooks/$(NAME) && sed -i -e 's/VERSION_NUMBER/v$(newVersionNumber)/g' README.md
	cd ppbooks/$(NAME) && sed -i -e 's/VERSION_NUMBER/v$(newVersionNumber)/g' book.json

	test -d books/$(NAME) || mkdir -p books/$(NAME)

	make check-summary
	make book-check-leftover-docublocks
	make book-check-restheader-leftovers
	make book-check-mdpp-leftovers

	cd ppbooks/$(NAME) && gitbook install
	cd ppbooks/$(NAME) && gitbook build ./ ./../../books/$(NAME)
	python deprecated.py

	make book-check-markdown-leftovers

clean-book:
	@rm -rvf books/$(NAME) ppbooks/$(NAME)

clean-book-intermediate:
	@rm -rvf ppbooks/$(NAME)


################################################################################
# Global targets


#************************************************************
# Check docublocks - checks whether docublock are 
#  - files in intermediate output directories and temporary
#    files are excludes (with # in their names)
#  - uniq in the source 
#  - all docublocks are used somewhere in the documentation
#  
check-docublocks:
	grep -R '@startDocuBlock' --include "*.h" --include "*.cpp" --include "*.js" --include "*.mdpp" . |\
		grep -v '@startDocuBlockInline' |\
		grep -v ppbook |\
		grep -v allComments.txt |\
		grep -v Makefile |\
		grep -v codeBlockReader.py |\
		grep -v generateMdFiles.py |\
		grep -v '.*~:.*' |\
		grep -v '.*#.*:.*' \
		   > /tmp/rawindoc.txt
	grep -R '@startDocuBlockInline' --include "*.h" --include "*.cpp" --include "*.js" --include "*.mdpp" . |\
		grep -v ppbook |\
		grep -v allComments.txt |\
		grep -v Makefile |\
		grep -v codeBlockReader.py |\
		grep -v generateMdFiles.py |\
		grep -v '.*~:.*' |\
		grep -v '.*#.*:.*' \
		   >> /tmp/rawindoc.txt
	cat /tmp/rawindoc.txt | sed  -e "s;.*ck ;;" -e "s;.*ne ;;" |sort -u > /tmp/indoc.txt
	grep -R '^/// @startDocuBlock' ../../lib ../../arangod ../../arangosh ../../js --include "*.h" --include "*.cpp" --include "*.js" --include "*.mdpp" |grep -v aardvark > /tmp/rawinprog.txt
# searching the Inline docublocks needs some more blacklisting:
	grep -R '@startDocuBlockInline' --include "*.h" --include "*.cpp" --include "*.js" --include "*.mdpp" . |\
		grep -v ppbook |\
		grep -v allComments.txt |\
		grep -v Makefile |\
		grep -v codeBlockReader.py |\
		grep -v generateMdFiles.py |\
		grep -v '.*~:.*' |\
		grep -v '.*#.*:.*' \
		   >> /tmp/rawinprog.txt
	echo "Generated: @startDocuBlockInline errorCodes">> /tmp/rawinprog.txt
	cat /tmp/rawinprog.txt |sed -e "s;.*ck ;;" -e "s;.*ne ;;" |sort > /tmp/inprog_raw.txt
	@cat /tmp/inprog_raw.txt |sort -u > /tmp/inprog.txt
	@if test "`cat /tmp/inprog.txt |wc -l`" -ne "`cat /tmp/inprog_raw.txt|wc -l`"; then \
		echo "Duplicate entry found in the source trees:"; \
		comm -3 /tmp/inprog_raw.txt /tmp/inprog.txt; \
		exit 1; \
	fi
	if test "`comm -3 /tmp/indoc.txt /tmp/inprog.txt |wc -l `" -ne 0; then \
		echo "Not all blocks were found on both sides:"; \
		echo "Documentation      |     Programcode:"; \
		comm -3 /tmp/indoc.txt /tmp/inprog.txt; \
		if test "`comm -2 -3 /tmp/indoc.txt /tmp/inprog.txt |wc -l`" -gt 0; then \
			echo "Documentation: "; \
			for grepit in `comm -2 -3 /tmp/indoc.txt /tmp/inprog.txt`; do \
				grep "$$grepit" /tmp/rawindoc.txt; \
			done; \
		fi; \
		if test "`comm -1 -3 /tmp/indoc.txt /tmp/inprog.txt |wc -l`" -gt 0; then \
			echo "Program code:"; \
			for grepit in `comm -1 -3 /tmp/indoc.txt /tmp/inprog.txt`; do \
				grep "$$grepit" /tmp/rawinprog.txt | sed "s;/// @startDocuBlock;\t\t;"; \
			done; \
		fi; \
		exit 1; \
	fi

clean-intermediate:
	make clean-book-intermediate NAME=Users


clean: clean-intermediate
	rm -f allComments.txt


.PHONY: build-books

build-books-keep-md:
	@test -d books || mkdir books
	python codeBlockReader.py
	make build-book NAME=Users

build-books: clean-intermediate build-books-keep-md check-docublocks
