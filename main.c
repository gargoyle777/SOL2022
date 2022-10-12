//TODO: check for error when accessing the array

int main( int argc, char* argv[] )
{
    int ac; //counts how many argument i have checked
    int nthread = 4;       //default is 4
    int qlen = 8;       //default is 8
    for( ac = 1; ac<argc; ac++) //0 is filename
    {
        if( argv[ac][0] == '-')
        {
            switch ( argv[ac][1] )
            {
            case 'n':       //number of thread
                ac++;
                nthread = atoi(argv[ac]);
                break;
            case 'q':       //concurrent line's length
                ac++;
                qlen = atoi(argv[ac]);
                break;
            case 'd':       //directory name
                /* code */      
                break;
            case 't':       //delay (default is 0)
                /* code */
                break;
            
            default:
                break;
            }
        }
    }
}