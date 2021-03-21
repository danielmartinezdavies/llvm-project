// RUN: %check_clang_tidy %s grppi-map-reduce %t


void f(){
int p[10]; 
int p2[10];
    for(int i = 0; i < 10; i++){
    	p[i] = 0;
    }
   // CHECK-MESSAGES: warning: Map pattern detected. Loop can be parallelized. [grppi-map-reduce]
   // CHECK-FIXES:grppi::map(grppi::dynamic_execution(), p, 10, p, [=](auto grppi_p){{{[[:space:]]*}}return{{[[:space:]]+}}0;{{[[:space:]]*}}});
   
   for(int i = 0; i < 10; i++){p[i] = 0;}
   // CHECK-MESSAGES: warning: Map pattern detected. Loop can be parallelized. [grppi-map-reduce]
   // CHECK-FIXES:grppi::map(grppi::dynamic_execution(),  p, 10, p, [=](auto grppi_p){return  0;});
    
 
    
    
}

// FIXME: Add something that doesn't trigger the check here.
void awesome_f2();


