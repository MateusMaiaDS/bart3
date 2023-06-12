#include<RcppArmadillo.h>
#include<vector>
// Creating the struct
struct Node;
struct modelParam;

struct modelParam {

        arma::mat x_train;
        arma::vec y;
        arma::mat x_test;
        arma::mat xcut;


        // BART prior param specification
        int n_tree;
        int d_var; // Dimension of variables in my base
        double alpha;
        double beta;
        double tau_mu;
        double tau;
        double a_tau;
        double d_tau;
        double nu;
        int node_min_size;

        // MCMC spec.
        int n_mcmc;
        int n_burn;

        // Create an indicator of accepted grown trees
        arma::vec move_proposal;
        arma::vec move_acceptance;

        // Create a boolean to only use stumps
        bool stump;

        // Defining the constructor for the model param
        modelParam(arma::mat x_train_,
                   arma::vec y_,
                   arma::mat x_test_,
                   int n_tree_,
                   int node_min_size_,
                   double alpha_,
                   double beta_,
                   double tau_mu_,
                   double tau_,
                   double a_tau_,
                   double d_tau_,
                   double n_mcmc_,
                   double n_burn_,
                   bool stump_);

};

// Creating a forest
class Forest {

public:
        std::vector<Node*> trees;

        Forest(modelParam &data);
        // ~Forest();
};



// Creating the node struct
struct Node {

     bool isRoot;
     bool isLeaf;
     Node* left;
     Node* right;
     Node* parent;
     arma::vec train_index;
     arma::vec test_index;

     // Branch parameters
     int var_split;
     double var_split_rule;
     double lower;
     double upper;
     double curr_weight; // indicates if the observation is within terminal node or not
     int depth = 0;

     // Leaf parameters
     double mu;

     // Storing sufficient statistics over the nodes
     double log_likelihood = 0.0;
     double r_sq_sum = 0;
     double r_sum = 0;

     int n_leaf = 0;
     int n_leaf_test = 0;


     // Creating the methods
     void addingLeaves(modelParam& data);
     void deletingLeaves();
     void Stump(modelParam& data);
     void updateWeight(const arma::mat X, int i);
     void getLimits(); // This function will get previous limit for the current var
     void sampleSplitVar(modelParam& data);
     bool isLeft();
     bool isRight();
     void grow(Node* tree, modelParam &data, arma::vec &curr_res);
     void prune(Node* tree, modelParam &data, arma::vec&curr_res);
     void change(Node* tree, modelParam &data, arma::vec&curr_res);

     void nodeLogLike(modelParam &data, arma::vec &curr_res);
     void displayCurrNode();

     Node(modelParam &data);
     ~Node();
};

// Creating a function to get the leaves
void leaves(Node* x, std::vector<Node*>& leaves); // This function gonna modify by address the vector of leaves
std::vector<Node*> leaves(Node*x);
// [[Rcpp::export]]
double rand_unif(){
        double rand_d = std::rand();
        return rand_d/RAND_MAX;
};
