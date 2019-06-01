/*
 *  MIT License
 *  
 *  Copyright (c) 2019 Arnold Schommer
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */
#ifndef PROFILING_h
#define PROFILING_h

class Profile { 
    private:
        uint32_t last_start;

    public:

        const char *label;
        uint32_t n_runs, sum_runtime, min_runtime, max_runtime;

        Profile(const char *label) {
            this->label = label;
            this->n_runs = this->sum_runtime = this->min_runtime = this->max_runtime = this->last_start = 0; 
        }

        inline void start(void) { this->last_start = micros(); }
        
        void done(void) {
            uint32_t runtime = micros() - this->last_start;
            if(this->last_start == 0)   return;
            this->n_runs++;
            this->sum_runtime += runtime;
            if(this->min_runtime == 0 || this->min_runtime > runtime)   this->min_runtime = runtime;
            if(this->max_runtime < runtime)   this->max_runtime = runtime;
            this->last_start = 0;
        }
        
        void print(void) {
            Serial.printf("%s:\n\truns\t%ld\n\tsum\t%d mus\n\tavg\t%ld mus\n\tmin\t%ld mus\n\tmax\t%ld mus\n",
                        this->label, this->n_runs, this->sum_runtime, this->n_runs ? this->sum_runtime/this->n_runs : 0,
                        this->min_runtime, this->max_runtime);
        }
        
        void tabular_head(void) {
            Serial.println("\nfunction                          #runs   sum us   avg us   min us   max us");
        }
        void tabular_print(void) {
            Serial.printf("%-30s %8ld %8d %8ld %8ld %8ld\n",
                        this->label, this->n_runs, this->sum_runtime, this->n_runs ? this->sum_runtime/this->n_runs : 0,
                        this->min_runtime, this->max_runtime);
        }
};

#define PROFILED_RUN(profile, call) { profile.start(); call; profile.done(); }

#endif PROFILING_h
