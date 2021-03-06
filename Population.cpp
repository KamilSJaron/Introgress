#include "Maths.h"
#include "Network.h"
#include "Indiv.h"
#include "Population.h"

#include <armadillo>
#include <cmath>
#include <vector>
#include <random>
#include <iostream>
#include <map>

using namespace std;
using namespace arma;


// Constructor definition
void Population::loadParams(Indiv indinit,
                            Indiv indscnd,
                            int nitr,
                            int nphns,
                            double eps,
                            double mu,
                            int n_gams,
                            rowvec phenopt,
                            double omg,
                            int nind,
                            double self,
                            double bckrte)
{
    ind_init = indinit;           // individual used to initiate population
    ind_scndprt = indscnd;        // individual used to initiate population
    niter_conv = nitr;            // number of iterations to check convergence
    n_phens = nphns;              // number of phenotypes to check convergence
    epsilon = eps;                // epsilon param to check convergence
    mut_rate = mu;                // mutation rate
    stock_gamete = n_gams;        // number of available gametes
    phen_opt = phenopt;           // optimal phenotype, for for fitness computation
    omega = omg;                  // omega parameter, for fitness computation
    n_indiv = nind;
    self_rate = self;
    backcross_rate = bckrte;
}

void Population::newPhenOpt(rowvec phenopt)
{
    phen_opt = phenopt;
}

void Population::populateIni()
{
    for(int ind_idx=0; ind_idx<n_indiv; ind_idx++)
    {
        myIndivs.push_back(ind_init);
        myNames.push_back(ind_idx);
        //cout << "Population::populateIni() ind" << ind_idx << "_deltaNets = " << sum(sum(myIndivs[ind_idx].getNetwork1() - myIndivs[ind_idx].getNetwork2())) << endl;
        //cout << "Population::populateIni(): phen = \n" << ind_init.getPhen() << endl;
    }
    // cout << "Population::populateIni(): ind_stock = " << myIndivs.size() << endl;
}


//void Population::spanGametes()
//{
//    int ind_stock = myIndivs.size();
//    for(int ind_idx=0; ind_idx<ind_stock; ind_idx++)
//    {
//    arma::mat w_gamete = myIndivs[ind_idx].getGamete(0);
//    myGametes.push_back(w_gamete);
//    }
//}


void Population::savePhenotypes(string phens_file)
{
    ofstream myfile;
    myfile.open (phens_file);

    // initialize variables
    // myFitnesses.clear(); // make sure we update those fitnesses
    int ind_stock = myIndivs.size();

    // print infos to cout if needed
    for(int ind_idx=0; ind_idx<ind_stock; ind_idx++)   // warning: iterate over remaining indvs, so use ind_stock
    {
        rowvec ind_phe = myIndivs[ind_idx].getPhen();
        myfile << ind_phe;
    }

    myfile.close();
}


void Population::saveNetworks(string networks_file)
{
    ofstream myfile;
    myfile.open (networks_file);

    // initialize variables
    // myFitnesses.clear(); // make sure we update those fitnesses
    int ind_stock = myIndivs.size();

    // print w1 to file
    for(int ind_idx=0; ind_idx<ind_stock; ind_idx++)   // warning: iterate over remaining indvs, so use ind_stock
    {
        mat w1 = myIndivs[ind_idx].getNetwork1();
        rowvec w1_vector = mat2vec(w1);
        myfile << w1_vector;
    }

    // print w2 to file
    for(int ind_idx=0; ind_idx<ind_stock; ind_idx++)   // warning: iterate over remaining indvs, so use ind_stock
    {
        mat w2 = myIndivs[ind_idx].getNetwork2();
        rowvec w2_vector = mat2vec(w2);
        myfile << w2_vector;
    }
    myfile.close();
}


void Population::loadNetworks(string networks_file)
{
    // load file
    mat W;
    W.load(networks_file);


    // get infos about this dataset
    int n_genes = sqrt(W.n_cols);
    int n_indiv = W.n_rows / 2;

    cout << "Population::loadNetworks() loading n_indiv = " << n_indiv << endl;

    // populate pop with corresponding individuals
    for(int ind_idx=0; ind_idx<n_indiv; ind_idx++)   // warning: iterate over remaining indvs, so use ind_stock
    {
        // get gene networks of focal individual
        mat w1 = vec2mat(W.row(ind_idx));
        mat w2 = vec2mat(W.row(ind_idx + n_indiv));

        // initiate individual with this
        Indiv ind_init;
        ind_init.loadParams(w1,
                            w2,
                            niter_conv,        // w1 and w2 are declared internally, to use for debug purposes only
                            n_phens,
                            epsilon,
                            mut_rate,
                            stock_gamete,
                            phen_opt,
                            omega);

        // save to myIndivs and myNames
        myIndivs.push_back(ind_init);
        myNames.push_back(ind_idx);
    }
}


void Population::getFitnesses(int verbose)
{
    // Clear existing vector
    myFitnesses.clear();

    /* Important to compute fitnesses to indivs contained in myIndivs
    hence use ind_stock because it gets updated.
    and *NOT* according to n_indivs (that is a constant)*/
    int ind_stock = myIndivs.size();

    // compute fitnesses over those guys
    // double minfit = 0;
    for(int ind_idx=0; ind_idx<ind_stock; ind_idx++)
    {
        double ind_fit = myIndivs[ind_idx].getFitn();
        myFitnesses.push_back(ind_fit);
        if(verbose == 1) cout << "Population::getFitnesses() Indiv " << ind_idx << "\t" << ind_fit << endl;
        // if(minfit > ind_fit) minfit = indfit
    }
}


Indiv Population::getOffspring(int verbose)
{
    // Initiate random samplers
    std::random_device rd;
    std::mt19937 gen(rd());
    uniform_real_distribution<> unif_proba(0, 1);


    //// Produce offsprings according to fitness
    // -> Initiate viability counters
    bool parents_ok = 0; //parent1 must be OK
    bool offsprg_ok = 0; //offspring must be OK

    // -> Draw parents, check their gametes and the produced descendant
    while(parents_ok == 0 && offsprg_ok == 0)
    {
        //// Initial checks
        // Check number of remaining individuals
        int ind_stock = myIndivs.size();
        if(verbose == 1) cout << "Population::getOffspring ind_stock = " << ind_stock << endl;
        if(ind_stock == 0)
        {
            cout << "Population::getOffspring STOP: Not enough parents left\n" << endl;
            Indiv ind_null;
            return(ind_null);
            break;
        }

        // Check if we still have enough gametes available to cross
        int gam_stock = 0;
        for(int ind_idx=0; ind_idx<ind_stock; ind_idx++)   // warning: iterate over remaining indvs, so use ind_stock
        {
            int gam_cnt = myIndivs[ind_idx].getGameteCnt();
            // cout << "Population::getOffspring gam_stock ind_idx " << ind_idx << " = " << gam_cnt << endl;
            gam_stock = gam_stock + gam_cnt;
        }
        if(verbose == 1) cout << "Population::getOffspring gam_stock = " << gam_stock << endl;

        if(gam_stock < 2)
        {
            cout << "Population::getOffspring STOP: Not enough gametes left\n" << endl;
            Indiv ind_null;
            return(ind_null);
            break;
        }

        // If there is one specimen left, with two gametes, force selfing
        if(ind_stock == 1 && gam_stock >= 2)
        {
            self_rate = 1;
        }


        //// Initial reproduction attempt
        /* Initiate random sampling according to fitnesses
        IMPORTANT: this distribution MUST be defined within the
        while loop, because the fitness values are updated each time
        we erase an Indiv from myIndivs, myFitnesses and myNames
        */
        if(verbose == 1) cout << "Population::getOffspring random draws" << endl;
        std::discrete_distribution<> rand_int_fitness (myFitnesses.begin(), myFitnesses.end());

        // Define couple of parents
        int p1_idx = rand_int_fitness(gen);        // find parent 1

        // Check for selfing events
        int p2_idx = p1_idx;                       // By default, parent 2 = parent 1, we are on a selfing figure case

        int self_cntdwn = 0;
        double self_event = unif_proba(gen);       // Change this matter of fact according to self_rate
        if(self_event > self_rate)
        {
            while(p1_idx == p2_idx && self_cntdwn < 10) // Include an escape
            {
                double backcross_event = unif_proba(gen);                // Enforce usage of reccurrent parent according to backcrossing rate
                if(backcross_event < backcross_rate)                     // If back-crossing event takes places:
                {
                    p2_idx = -1;                                             // pick recurrent parent
                }
                else
                {
                    p2_idx = rand_int_fitness(gen);                         // else, pick second parent in main pop, according to fitness
                    self_cntdwn = self_cntdwn + 1;
                    if(verbose == 1) cout << "Population::getOffspring self_cntdwn = " << self_cntdwn << endl;
                }
            }
        }

        if(verbose == 1) cout << "Population::getOffspring random draws = done" << endl;


        // For cout display only: make sure that indiv names are not shown as indexes
        int p1_name = myNames[p1_idx];
        int p2_name;
        if(p2_idx == -1)
        {
            p2_name = -1;
        }
        else
        {
            p2_name = myNames[p2_idx];
        }

        if(verbose == 1) cout << "Population::getOffspring trying parent pair = " << p1_name << "_" << p2_name << endl;


        // Get gametes w1_gamete and w2_gamete
        arma::mat w1_gamete = myIndivs[p1_idx].getGamete(0);         // Generate gamete from parent 1, via getGamete() method
        arma::mat w2_gamete;
        if(p2_idx == -1)
        {
            w2_gamete = ind_scndprt.getGamete(1);          // Generate gamete from parent 2 (recurrent parent, if backcross), via getGamete() method
                                                           // NB: this parent is a infinite provider of gametes (infinite = 1).
        // if(verbose == 1) cout << "Population::getOffspring getting BACKCROSS gamete = \n" << w2_gamete << endl;
        }
        else
        {
            w2_gamete = myIndivs[p2_idx].getGamete(0);     // Generate gamete from parent 2, via getGamete() method
        }


        // Check gamete cnts of those individuals
        int p1_gcnt = myIndivs[p1_idx].getGameteCnt();               // Get gamete count for parent 1, via getGameteCnt() method
        int p2_gcnt;
        if(p2_idx == -1)
            {
                p2_gcnt = stock_gamete;               // Get gamete count for parent 2, via getGameteCnt() method
            }
            else
            {
                p2_gcnt = myIndivs[p2_idx].getGameteCnt();               // Get gamete count for parent 2, via getGameteCnt() method
            }
        if(verbose == 1) cout << "Population::getOffspring getting p2_gcnt = " << p2_gcnt << endl;

        /* If selfing, and that p1 had only 1 gamete, p2 will have -1 gametes left.
        We cannot allow this and kill this cross.
        */
        bool killcross = 1;
        if(p1_idx == p2_idx)
        {
            if(p2_gcnt < 0)
            {
            killcross = 0;
            p1_gcnt = 0;
            p2_gcnt = 0;
            }
        }

        /* Discard individuals having exhausted their gamete stock,
        so that they cannot contribute to the reproduction effort*/
        if(p1_gcnt == 0)
        {
            myIndivs.erase(myIndivs.begin() + p1_idx);               // Erase parent 1 from myIndivs, myFitnesses and myNames vectors
            myFitnesses.erase(myFitnesses.begin() + p1_idx);
            myNames.erase(myNames.begin() + p1_idx);
            if(verbose == 1) cout << "Population::getOffspring erasing parent1: p1 = " << p1_name << "\t ind_stock = " << myIndivs.size() << endl;
        }

        if(p2_gcnt == 0 && p1_idx != p2_idx)                         // Erase parent 2 from myIndivs, myFitnesses and myNames vectors, unless if we are in a selfing case
        {
            /* technical trick: since we erase positions
            in myIndivs and his friends, we have to account that
            p2_idx might not be targetting the correct individual
            if p1_idx is smaller than p2_idx. We have to correct
            for this using a shifter. NB: find how we can erase two
            non contiguous elements in those vectors.
            */
            int shift = 0;
            if(p1_idx < p2_idx)
            {
                shift = -1;
            }
            myIndivs.erase(myIndivs.begin() + p2_idx + shift);
            myFitnesses.erase(myFitnesses.begin() + p2_idx + shift);
            myNames.erase(myNames.begin() + p2_idx + shift);
            if(verbose == 1) cout << "Population::getOffspring erasing parent1; p2 = " << p2_name << "\t ind_stock = " << myIndivs.size() << endl;
        }


        // Check convergence of w1_gamete and w2_gamete
        if(w1_gamete.has_nan() || w2_gamete.has_nan())
        {
            parents_ok = killcross * 0;                                          // w1 and / or w2 gamete is not convergent, make sure that while loop is not satisfied
            if(verbose == 1)
            {
                cout << "Population::getOffspring non-viable gamete" << endl;
            }
        }
        else
        {
            parents_ok = killcross * 1;                                          // Both w1 and w2 gametes are convergent, make sure that while loop is satisfied
        }


        // Unite gametes into new offspring, if both gametes are convergent
        if(parents_ok == 1)
        {
            Indiv ind_offsprg;                                       // initiate Indiv class
            ind_offsprg.loadParams(w1_gamete,                        // with w1_gamete and w2_gamete as gene networks
                                   w2_gamete,
                                   niter_conv,
                                   n_phens,
                                   epsilon,
                                   mut_rate,
                                   stock_gamete,
                                   phen_opt,
                                   omega);


            // Check offspring convergence
            arma::rowvec phen_offsprg = ind_offsprg.getPhen();

            if(phen_offsprg.has_nan())
            {
                offsprg_ok = 0;   // the obtained offspring is not viable, make sure that while loop is not satisfied
                parents_ok = 0;   // WARNING: we have to make sure that parents_ok is reinitialised properly.
                if(verbose == 1) cout << "Population::getOffspring non-viable offspring" << endl;
            }
            else
            {
                offsprg_ok = 1;                                       // the obtained offspring is not viable, make sure that while loop is not satisfied
                if(verbose == 1) cout << "Population::getOffspring trying parent pair = " << p1_name << "_" << p2_name << " OK\n" << endl;
                return(ind_offsprg);
            }
        }
    }
}


void Population::runGenerations(int n_generations, int verbose)
{
    // open report file
    ofstream myfile;
    myfile.open ("DistToOpt.txt");

    // Loop over all generations to produce
    for(int gen_idx=0; gen_idx<n_generations; gen_idx++)
    {
        // Initiate temporary vectors
        std::vector <Indiv> myInds_tmp;
        std::vector <int> myNms_tmp;

        // Compute fitnesses
        Population::getFitnesses(0);

        // Loop over all individuals to produce
        int killgen = 0;
        for(int ind_idx=0; ind_idx<n_indiv; ind_idx++)
        {
            // produce offspring
            Indiv ind_tmp = getOffspring(0);
            // cout << "Population::runGeneration: ind_tmp.getNetwork1() =\n" << ind_tmp.getNetwork1() << endl;

            // check it was produced correctly
            if(ind_tmp.getNetwork1().n_rows == 0){ // nope: the gamete / indiv pool is empty, we can skip the end of the generation
                cout << "Population::runGeneration: STOP could not finish generation" << endl;
                killgen = 1;
                break;
            }
            else // OK, keep going
            {
                myInds_tmp.push_back(ind_tmp); // 0 = silent mode
                myNms_tmp.push_back(ind_idx);
            }
        }

        // Check if pop size is shrinking
        int pop_size = myInds_tmp.size();
        double pop_ratio = pop_size / n_indiv;
        //cout << "Population::runGeneration: " << gen_idx << " pop_ratio = " << pop_ratio << endl;
        if(pop_ratio < 0.5 || killgen == 1) // yes it does: kill the ongoing loop
        {
            cout << "Population::runGeneration: STOP: population went to extinction" << endl;
            break;
        }

        // Save to myIndivs and myNames
        myIndivs.clear();
        myIndivs.swap(myInds_tmp);
        myNames.clear();
        myNames.swap(myNms_tmp);

        // print infos to screen
        if(verbose == 1)
        {
            // compute average phenotype
            int n_genes = myIndivs[0].getNetwork1().n_rows;
            rowvec phen_avg(n_genes, fill::zeros);              // initialize phen_avg
            double cnt = 0;                                     // initialize number of viable samples

            // loop over all specimens and sum to phen_avg
            for(int ind_idx=0; ind_idx<n_indiv; ind_idx++)
            {
                // get specimen phenotype
                rowvec phen_last = myIndivs[ind_idx].getPhen();
                if(phen_last.has_nan() == 0)
                {
                    phen_avg = phen_avg + phen_last;
                    cnt = cnt + 1;
                }
            }

            // divide by number of specimens to get average phenotype
            rowvec phen_cnt(n_genes);
            phen_cnt.fill(cnt);
            phen_avg = phen_avg / phen_cnt;

            // compute distance to phen_opt
            double delta = sum(abs(phen_avg - phen_opt)) / n_genes;

            // print infos to screen
            cout << "Population::runGeneration: " << gen_idx << " Avg. dist to phen_opt = " << delta << endl;
            myfile << delta << "\n";
        }
    }
    myfile.close();
}



