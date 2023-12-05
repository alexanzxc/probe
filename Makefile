babka: babka.cpp
	g++ -O0 -I /home/vkarakos/Data-DellXPS9310/coding/TLB-microbenchmarking/papi-7.0.1-instdir/include/ babka.cpp -L/home/vkarakos/Data-DellXPS9310/coding/TLB-microbenchmarking/papi-7.0.1-instdir/lib -lpapi -o babka

clean:
	rm -f babka
