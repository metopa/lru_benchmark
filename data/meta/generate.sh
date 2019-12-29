rm *.eps
gnuplot -e "filename='p4_1000_1_p.tsv'"    -e "plotname='P4 1/1000 1 thread'"   meta-hitrate.gnuplot
gnuplot -e "filename='p4_1000_1_t.tsv'"    -e "plotname='P4 1/1000 1 thread'"   meta.gnuplot
gnuplot -e "filename='p4_1000_32_p.tsv'"   -e "plotname='P4 1/1000 32 threads'" meta-hitrate.gnuplot
gnuplot -e "filename='p4_1000_32_t.tsv'"   -e "plotname='P4 1/1000 32 threads'" meta.gnuplot
gnuplot -e "filename='p4_10_1_p.tsv'"      -e "plotname='P4 1/10 1 thread'"     meta-hitrate.gnuplot
gnuplot -e "filename='p4_10_1_t.tsv'"      -e "plotname='P4 1/10 1 thread'"     meta.gnuplot
gnuplot -e "filename='p4_10_32_p.tsv'"     -e "plotname='P4 1/10 32 threads'"   meta-hitrate.gnuplot
gnuplot -e "filename='p4_10_32_t.tsv'"     -e "plotname='P4 1/10 32 threads'"   meta.gnuplot

gnuplot -e "filename='p8_10_1_p.tsv'"      -e "plotname='P8 1/10 1 thread'"     meta-hitrate.gnuplot
gnuplot -e "filename='p8_10_1_t.tsv'"      -e "plotname='P8 1/10 1 thread'"     meta.gnuplot
gnuplot -e "filename='p8_10_32_p.tsv'"     -e "plotname='P8 1/10 32 threads'"   meta-hitrate.gnuplot
gnuplot -e "filename='p8_10_32_t.tsv'"     -e "plotname='P8 1/10 32 threads'"   meta.gnuplot
gnuplot -e "filename='p8_238_1_p.tsv'"     -e "plotname='P8 1/238 1 thread'"    meta-hitrate.gnuplot
gnuplot -e "filename='p8_238_1_t.tsv'"     -e "plotname='P8 1/238 1 thread'"    meta.gnuplot
gnuplot -e "filename='p8_238_32_p.tsv'"    -e "plotname='P8 1/238 32 threads'"  meta-hitrate.gnuplot
gnuplot -e "filename='p8_238_32_t.tsv'"    -e "plotname='P8 1/238 32 threads'"  meta.gnuplot

gnuplot -e "filename='wiki_1000_1_p.tsv'"  -e "plotname='Wikipedia 1/1000 1 thread'"   meta-hitrate.gnuplot
gnuplot -e "filename='wiki_1000_1_t.tsv'"  -e "plotname='Wikipedia 1/1000 1 thread'"   meta.gnuplot
gnuplot -e "filename='wiki_1000_32_p.tsv'" -e "plotname='Wikipedia 1/1000 32 threads'" meta-hitrate.gnuplot
gnuplot -e "filename='wiki_1000_32_t.tsv'" -e "plotname='Wikipedia 1/1000 32 threads'" meta.gnuplot
gnuplot -e "filename='wiki_10_1_p.tsv'"    -e "plotname='Wikipedia 1/10 1 thread'"     meta-hitrate.gnuplot
gnuplot -e "filename='wiki_10_1_t.tsv'"    -e "plotname='Wikipedia 1/10 1 thread'"     meta.gnuplot
gnuplot -e "filename='wiki_10_32_p.tsv'"   -e "plotname='Wikipedia 1/10 32 threads'"   meta-hitrate.gnuplot
gnuplot -e "filename='wiki_10_32_t.tsv'"   -e "plotname='Wikipedia 1/10 32 threads'"   meta.gnuplot

for f in *.tsv.eps; do 
    mv -- "$f" "${f%.tsv.eps}.eps"
done