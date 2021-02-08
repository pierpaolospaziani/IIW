void DieWithError(char *errorMessage){
    perror(errorMessage);
    exit(1);
}


int is_lost(float loss_rate) {
    double rv;
    rv = drand48();
    if (rv < loss_rate){
        return(1);
    } else {
        return(0);
    }
}
