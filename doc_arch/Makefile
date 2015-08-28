#
# Usage:
# make          # make dvi, ps, and pdf
# make dvi      # make dvi
# make ps       # make ps (and dvi)
# make pdf      # make pdf
#

MAIN		= exanodes_arch
IMAGE_DIR	= ./images
SOURCES		= $(wildcard ./*.tex)
EPSFIGURES	= $(patsubst %.dia,%.eps,$(wildcard ${IMAGE_DIR}/*.dia))	\
		  $(patsubst %.pic,%.eps,$(wildcard ${IMAGE_DIR}/*.pic))
PNGFIGURES	= $(patsubst %.dia,%.png,$(wildcard ${IMAGE_DIR}/*.dia)) 	\
		  $(patsubst %.pic,%.png,$(wildcard ${IMAGE_DIR}/*.pic))
PSTEXFIGURES	= $(patsubst %.fig,%.pstex,$(wildcard ${IMAGE_DIR}/*.fig))	\
		  $(patsubst %.fig,%.pstex_t,$(wildcard ${IMAGE_DIR}/*.fig))
GIT_REVISION	= svn_revision.sty
LATEX_STYLE	= seanodes_spec.sty
SEANODES_LOGO	= seanodeslogo.eps
UML_PIC_MACRO	= sequence.pic

BIBFILE		= $(wildcard ./*.bib)
versionned	= $(wildcard ./.svn)

all: dvi ps pdf

eps:	${EPSFIGURES}
png:	${PNGFIGURES}
dvi: 	svn_export eps png ${MAIN}.dvi
pdf: 	ps ${MAIN}.pdf
ps: 	dvi ${MAIN}.ps

${MAIN}.dvi : ${GIT_REVISION} ${SOURCES} ${EPSFIGURES} ${PSTEXFIGURES} ${BIBFILE}
	latex -file-line-error ${MAIN}
	@if test -e ${MAIN}.bib; then	\
		bibtex ${MAIN};		\
		latex ${MAIN};		\
	fi
	@while ( grep "Rerun to get " ${MAIN}.log > /dev/null ||	\
		grep "Citation(s) may have changed" ${MAIN}.log > /dev/null ); do	\
		echo '** Re-running LaTeX **';		\
		latex ${MAIN};				\
	done

${MAIN}.pdf : ${MAIN}.ps
	# running ps2pdf
	ps2pdf -sPAPERSIZE=a4 ${MAIN}.ps ${MAIN}.pdf


${MAIN}.ps : ${MAIN}.dvi
	# running dvips
	dvips ${MAIN}.dvi -o ${MAIN}.ps


svn_export: \
           ${LATEX_STYLE} \
	   ${IMAGE_DIR}/${SEANODES_LOGO}

${LATEX_STYLE}:
	# get standard Seanodes LaTex style directly from SVN
	svn export svn+ssh://fusion/svn/activity/common/latex/${LATEX_STYLE}

${IMAGE_DIR}/${SEANODES_LOGO}:
	mkdir -p ${IMAGE_DIR}
	# get Seanodes logo directly from SVN
	svn export svn+ssh://fusion/svn/activity/common/images/${SEANODES_LOGO} ${IMAGE_DIR}/${SEANODES_LOGO}
	# get UML Pic macros directly from SVN
	svn export svn+ssh://fusion/svn/activity/common/images/${UML_PIC_MACRO}


${GIT_REVISION}:
	# build LaTex variable defining style to insert SVN revision and SVN date
	# Depending whether the directory is versionned or not
	echo versionned = ${versionned}
	echo '\makeatletter' > ${GIT_REVISION}
ifneq (${versionned}, )
	echo '\def\@svnrevision{rev. ' >> ${GIT_REVISION}
	LC_ALL=C svnversion -cn . | sed 's/.*://' >> ${GIT_REVISION}
	echo '}' >> ${GIT_REVISION}
	echo '\date{'>> ${GIT_REVISION}
	date +'%d %B %Y' -d `svn info --xml ${MAIN}.tex | grep "<date>" | sed  -e 's/<date>//' -e 's/T.*//'`>> ${GIT_REVISION}
	echo '}' >> ${GIT_REVISION}
else
	echo '\def\@svnrevision{rev. NONE}' >> ${GIT_REVISION}
	echo '\date{'>> ${GIT_REVISION}
	date +'%d %B %Y' >> ${GIT_REVISION}
	echo '}' >> ${GIT_REVISION}
endif
	echo '\makeatother' >> ${GIT_REVISION}

clean:
	rm -f ${IMAGE_DIR}/${SEANODES_LOGO}
	rm -f ${EPSFIGURES}
	rm -f ${PNGFIGURES}
	rm -f ${PSTEXFIGURES}
	rm -f ${LATEX_STYLE}
	rm -f ${GIT_REVISION}
	rm -f ${UML_PIC_MACRO}
	rm -f ./*.aux
	rm -f ./*.bbl
	rm -f ./*.blg
	rm -f ./*.lof
	rm -f ./*.out
	rm -f ./*.toc
	rm -f ./*.log
	rm -f ./*.dvi
	rm -f ./${MAIN}.ps
	rm -f ./${MAIN}.pdf
	rm -f ./*.tex~

#
# (re)Make .eps if .dia is newer
#
%.eps : %.dia
	#Creates .eps file from .dia file
	dia -t eps $*.dia

%.eps : %.pic
	#Creates .eps file from .pic file
	pic2plot -Tps $*.pic > $*.eps

#
# (re)Make .pstex* if .fig is newer
#
%.pstex : %.fig
	#Creates .pstex and .pstex_t files from .fig file
	fig2dev -L pstex $*.fig $*.pstex
	fig2dev -L pstex_t -p $*.pstex $*.fig $*.pstex_t

#
# (re)Make .png if .dia is newer
#
%.png : %.dia
	#Creates .png file from .dia file
	dia -s 1024 -t png $*.dia

%.png : %.pic
	#Creates .png file from .pic file
	pic2plot -Tpng $*.pic > $*.png
