#include "bart3.h"
#include <iomanip>
#include<cmath>
#include <random>
#include <RcppArmadillo.h>
using namespace std;

// =====================================
// Statistics Function
// =====================================







// //[[Rcpp::export]]
arma::mat sum_exclude_col(arma::mat mat, int exclude_int){

        // Setting the sum matrix
        arma::mat m(mat.n_rows,1);

        if(exclude_int==0){
                m = sum(mat.cols(1,mat.n_cols-1),1);
        } else if(exclude_int == (mat.n_cols-1)){
                m = sum(mat.cols(0,mat.n_cols-2),1);
        } else {
                m = arma::sum(mat.cols(0,exclude_int-1),1) + arma::sum(mat.cols(exclude_int+1,mat.n_cols-1),1);
        }

        return m;
}



// Initialising the model Param
modelParam::modelParam(arma::mat x_train_,
                       arma::vec y_,
                       arma::mat x_test_,
                       arma::mat xcut_,
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
                       bool stump_){


        // Assign the variables
        x_train = x_train_;
        y = y_;
        x_test = x_test_;
        xcut = xcut_;
        n_tree = n_tree_;
        node_min_size = node_min_size_;
        alpha = alpha_;
        beta = beta_;
        tau_mu = tau_mu_;
        tau = tau_;
        a_tau = a_tau_;
        d_tau = d_tau_;
        n_mcmc = n_mcmc_;
        n_burn = n_burn_;

        // Grow acceptation ratio
        move_proposal = arma::vec(5,arma::fill::zeros);
        move_acceptance = arma::vec(5,arma::fill::zeros);

        stump = stump_; // Checking if only restrict the model to stumps

}

// Initialising a node
Node::Node(modelParam &data){
        isLeaf = true;
        isRoot = true;
        left = NULL;
        right = NULL;
        parent = NULL;
        train_index = arma::vec(data.x_train.n_rows,arma::fill::ones)*(-1);
        test_index = arma::vec(data.x_test.n_rows,arma::fill::ones)*(-1) ;

        var_split = -1;
        var_split_rule = -1.0;
        lower = 0.0;
        upper = 1.0;
        mu = 0.0;
        n_leaf = 0.0;
        n_leaf_test = 0;
        log_likelihood = 0.0;
        depth = 0;


}

Node::~Node() {
        if(!isLeaf) {
                delete left;
                delete right;
        }
}

// Initializing a stump
void Node::Stump(modelParam& data){

        // Changing the left parent and right nodes;
        left = this;
        right = this;
        parent = this;
        // n_leaf  = data.x_train.n_rows;

        // Updating the training index with the current observations
        for(int i=0; i<data.x_train.n_rows;i++){
                train_index[i] = i;
        }

        // Updating the same for the test observations
        for(int i=0; i<data.x_test.n_rows;i++){
                test_index[i] = i;
        }

}

void Node::addingLeaves(modelParam& data){

     // Create the two new nodes
     left = new Node(data); // Creating a new vector object to the
     right = new Node(data);
     isLeaf = false;

     // Modifying the left node
     left -> isRoot = false;
     left -> isLeaf = true;
     left -> left = left;
     left -> right = left;
     left -> parent = this;
     left -> var_split = 0;
     left -> var_split_rule = -1.0;
     left -> lower = 0.0;
     left -> upper = 1.0;
     left -> mu = 0.0;
     left -> log_likelihood = 0.0;
     left -> n_leaf = 0.0;
     left -> depth = depth+1;
     left -> train_index = arma::vec(data.x_train.n_rows,arma::fill::ones)*(-1);
     left -> test_index = arma::vec(data.x_test.n_rows,arma::fill::ones)*(-1);

     right -> isRoot = false;
     right -> isLeaf = true;
     right -> left = right; // Recall that you are saving the address of the right node.
     right -> right = right;
     right -> parent = this;
     right -> var_split = 0;
     right -> var_split_rule = -1.0;
     right -> lower = 0.0;
     right -> upper = 1.0;
     right -> mu = 0.0;
     right -> log_likelihood = 0.0;
     right -> n_leaf = 0.0;
     right -> depth = depth+1;
     right -> train_index = arma::vec(data.x_train.n_rows,arma::fill::ones)*(-1);
     right -> test_index = arma::vec(data.x_test.n_rows,arma::fill::ones)*(-1);


     return;

}

// Creating boolean to check if the vector is left or right
bool Node::isLeft(){
        return (this == this->parent->left);
}

bool Node::isRight(){
        return (this == this->parent->right);
}

// This functions will get and update the current limits for this current variable
void Node::getLimits(){

        // Creating  a new pointer for the current node
        Node* x = this;
        // Already defined this -- no?
        lower = 0.0;
        upper = 1.0;
        // First we gonna check if the current node is a root or not
        bool tree_iter = x->isRoot ? false: true;
        while(tree_iter){
                bool is_left = x->isLeft(); // This gonna check if the current node is left or not
                x = x->parent; // Always getting the parent of the parent
                tree_iter = x->isRoot ? false : true; // To stop the while
                if(x->var_split == var_split){
                        tree_iter = false ; // This stop is necessary otherwise we would go up til the root, since we are always update there is no prob.
                        if(is_left){
                                upper = x->var_split_rule;
                                lower = x->lower;
                        } else {
                                upper = x->upper;
                                lower = x->var_split_rule;
                        }
                }
        }
}


void Node::displayCurrNode(){

                std::cout << "Node address: " << this << std::endl;
                std::cout << "Node parent: " << parent << std::endl;

                std::cout << "Cur Node is leaf: " << isLeaf << std::endl;
                std::cout << "Cur Node is root: " << isRoot << std::endl;
                std::cout << "Cur The split_var is: " << var_split << std::endl;
                std::cout << "Cur The split_var_rule is: " << var_split_rule << std::endl;

                return;
}


void Node::deletingLeaves(){

     // Should I create some warn to avoid memoery leak
     //something like it will only delete from a nog?
     // Deleting
     delete left; // This release the memory from the left point
     delete right; // This release the memory from the right point
     left = this;  // The new pointer for the left become the node itself
     right = this; // The new pointer for the right become the node itself
     isLeaf = true;

     return;

}
// Getting the leaves (this is the function that gonna do the recursion the
//                      function below is the one that gonna initialise it)
void get_leaves(Node* x,  std::vector<Node*> &leaves_vec) {

        if(x->isLeaf){
                leaves_vec.push_back(x);
        } else {
                get_leaves(x->left, leaves_vec);
                get_leaves(x->right,leaves_vec);
        }

        return;

}



// Initialising a vector of nodes in a standard way
std::vector<Node*> leaves(Node* x) {
        std::vector<Node*> leaves_init(0); // Initialising a vector of a vector of pointers of nodes of size zero
        get_leaves(x,leaves_init);
        return(leaves_init);
}

// Sweeping the trees looking for nogs
void get_nogs(std::vector<Node*>& nogs, Node* node){
        if(!node->isLeaf){
                bool bool_left_is_leaf = node->left->isLeaf;
                bool bool_right_is_leaf = node->right->isLeaf;

                // Checking if the current one is a NOGs
                if(bool_left_is_leaf && bool_right_is_leaf){
                        nogs.push_back(node);
                } else { // Keep looking for other NOGs
                        get_nogs(nogs, node->left);
                        get_nogs(nogs, node->right);
                }
        }
}

// Creating the vectors of nogs
std::vector<Node*> nogs(Node* tree){
        std::vector<Node*> nogs_init(0);
        get_nogs(nogs_init,tree);
        return nogs_init;
}



// Initializing the forest
Forest::Forest(modelParam& data){

        // Creatina vector of size of number of trees
        trees.resize(data.n_tree);
        for(int  i=0;i<data.n_tree;i++){
                // Creating the stump for each tree
                trees[i] = new Node(data);
                // Filling up each stump for each tree
                trees[i]->Stump(data);
        }
}

// Function to delete one tree
// Forest::~Forest(){
//         for(int  i=0;i<trees.size();i++){
//                 delete trees[i];
//         }
// }

// Selecting a random node
Node* sample_node(std::vector<Node*> leaves_){

        // Getting the number of leaves
        int n_leaves = leaves_.size();
        // return(leaves_[std::rand()%n_leaves]);
        if((n_leaves == 0) || (n_leaves==1) ){
             return leaves_[0];
        } else {
             return(leaves_[arma::randi(arma::distr_param(0,(n_leaves-1)))]);
        }

}

// Grow a tree for a given rule
void grow(Node* tree, modelParam &data, arma::vec &curr_res){

        // Getting the number of terminal nodes
        std::vector<Node*> t_nodes = leaves(tree) ;
        std::vector<Node*> nog_nodes = nogs(tree);

        // Selecting one node to be sampled
        Node* g_node = sample_node(t_nodes);

        // Store all old quantities that will be used or not
        int old_var_split = g_node->var_split;
        int old_var_split_rule = g_node->var_split_rule;

        // // Calculating the whole likelihood fo the tree
        for(int i = 0; i < t_nodes.size(); i++){
                // cout << "Error gpNodeLogLike" << endl;
                t_nodes[i]->updateResiduals(data, curr_res); // Do I need to do this?
        }

        // Calculating the likelihood of the current grown node
        g_node->nodeLogLike(data,curr_res);
        // cout << "LogLike Node ok Grow" << endl;

        // Adding the leaves
        g_node->addingLeaves(data);

        bool no_valid_node = false;
        int p_try = 0;

        // Trying to find a cutpoint
        arma::vec split_candidates = arma::shuffle(arma::regspace(0,1,data.x_train.n_cols-1));
        Rcpp::NumericVector valid_cutpoint;

        while(!no_valid_node){
                g_node->var_split = split_candidates(p_try);

                Rcpp::NumericVector var_split_range;

                // Getting the maximum and the minimum;
                for(int i = 0; i < g_node->n_leaf; i++){
                        var_split_range.push_back(data.x_train(g_node->train_index[i],g_node->var_split));
                }

                // Getting the minimum and the maximum;
                double max_rule = max(var_split_range);
                double min_rule = min(var_split_range);

                for(int cut = 0; cut < data.xcut.n_rows; cut++ ){
                        if((data.xcut(cut,g_node->var_split)>min_rule) & (data.xcut(cut,g_node->var_split)<max_rule)){
                                valid_cutpoint.push_back(data.xcut(cut,g_node->var_split));
                        }
                }

                if(valid_cutpoint.size()==0){
                        p_try++;
                        if(p_try>=data.x_train.n_cols){
                                no_valid_node = true;
                        };
                } else {
                        break; // Go out from the while
                }
        }

        if(no_valid_node){
        // Returning to the old values
                g_node->var_split = old_var_split;
                g_node->var_split_rule = old_var_split_rule;

                g_node->deletingLeaves();
                return;
        }

        // Selecting a rule (here I'm actually selecting the var split rule);
        g_node->var_split_rule = valid_cutpoint[arma::randi(arma::distr_param(0,valid_cutpoint.size()))];
        // cout << "The current var split rule is: " << g_node->var_split_rule << endl;

        // Create an aux for the left and right index
        int train_left_counter = 0;
        int train_right_counter = 0;

        int test_left_counter = 0;
        int test_right_counter = 0;

        // Create a matrix to store the new x_{j} and then get information of min and max
        arma::mat x_new_left(g_node->n_leaf, data.x_train.n_cols);
        arma::mat x_new_right(g_node->n_leaf,data.x_train.n_cols);

        // Updating the left and the right nodes
        for(int i = 0;i<data.x_train.n_rows;i++){
                if(g_node -> train_index[i] == -1 ){
                        g_node->left->n_leaf = train_left_counter;
                        g_node->right->n_leaf = train_right_counter;
                        break;
                }
                if(data.x_train(g_node->train_index[i],g_node->var_split)<=g_node->var_split_rule){
                        g_node->left->train_index[train_left_counter] = g_node->train_index[i];
                        x_new_left.row(train_left_counter) = data.x_train.row(g_node->train_index[i]);
                        train_left_counter++;
                } else {
                        g_node->right->train_index[train_right_counter] = g_node->train_index[i];
                        x_new_right.row(train_right_counter) = data.x_train.row(g_node->train_index[i]);
                        train_right_counter++;
                }

        }



        // Updating the left and right nodes for the
        for(int i = 0;i<data.x_test.n_rows; i++){
                if(g_node -> test_index[i] == -1){
                        g_node->left->n_leaf_test = test_left_counter;
                        g_node->right->n_leaf_test = test_right_counter;
                        break;
                }
                if(data.x_test(g_node->test_index[i],g_node->var_split)<=g_node->var_split_rule){
                        g_node->left->test_index[test_left_counter] = g_node->test_index[i];
                        test_left_counter++;
                } else {
                        g_node->right->test_index[test_right_counter] = g_node->test_index[i];
                        test_right_counter++;
                }
        }

        // If is a root node
        if(g_node->isRoot){
                g_node->left->n_leaf = train_left_counter;
                g_node->right->n_leaf = train_right_counter;
                g_node->left->n_leaf_test = test_left_counter;
                g_node->right->n_leaf_test = test_right_counter;
        }

        // Avoiding nodes lower than the node_min
        if((g_node->left->n_leaf<data.node_min_size) || (g_node->right->n_leaf<data.node_min_size) ){

                // cout << " NODES" << endl;
                // Returning to the old values
                g_node->var_split = old_var_split;
                g_node->var_split_rule = old_var_split_rule;


                g_node->deletingLeaves();
                return;
        }




        // Updating the loglikelihood for those terminal nodes
        // cout << "Calculating likelihood of the new node on left" << endl;
        // cout << " ACCEPTED" << endl;
        g_node->left->updateResiduals(data,curr_res);
        g_node->right->updateResiduals(data,curr_res);


        // Calculating the likelihood on the new node on the left
        g_node->left->nodeLogLike(data, curr_res);
        // cout << "Calculating likelihood of the new node on right" << endl;
        g_node->right->nodeLogLike(data, curr_res);
        // cout << "NodeLogLike ok again" << endl;


        // Calculating the prior term for the grow
        double tree_prior = log(data.alpha*pow((1+g_node->depth),-data.beta)) +
                log(1-data.alpha*pow((1+g_node->depth+1),-data.beta)) + // Prior of left node being terminal
                log(1-data.alpha*pow((1+g_node->depth+1),-data.beta)) - // Prior of the right noide being terminal
                log(1-data.alpha*pow((1+g_node->depth),-data.beta)); // Old current node being terminal

        // Getting the transition probability
        double log_transition_prob = log((0.3)/(nog_nodes.size()+1)) - log(0.3/t_nodes.size()); // 0.3 and 0.3 are the prob of Prune and Grow, respectively

        // Calculating the loglikelihood for the new branches
        double new_tree_log_like = - g_node->log_likelihood + g_node->left->log_likelihood + g_node->right->log_likelihood ;

        // Calculating the acceptance ratio
        double acceptance = exp(new_tree_log_like  + log_transition_prob + tree_prior);

        if(data.stump){
                acceptance = acceptance*(-1);
        }

        // Keeping the new tree or not
        if(arma::randu(arma::distr_param(0.0,1.0)) < acceptance){
                // Do nothing just keep the new tree
                data.move_acceptance(0)++;
        } else {
                // Returning to the old values
                g_node->var_split = old_var_split;
                g_node->var_split_rule = old_var_split_rule;

                g_node->deletingLeaves();
        }

        return;

}


// Pruning a tree
void prune(Node* tree, modelParam&data, arma::vec &curr_res){


        // Getting the number of terminal nodes
        std::vector<Node*> t_nodes = leaves(tree);

        // Can't prune a root
        if(t_nodes.size()==1){
                // cout << "Nodes size " << t_nodes.size() <<endl;
                t_nodes[0]->updateResiduals(data,curr_res);
                t_nodes[0]->nodeLogLike(data, curr_res);
                return;
        }

        std::vector<Node*> nog_nodes = nogs(tree);

        // Selecting one node to be sampled
        Node* p_node = sample_node(nog_nodes);


        // Calculating the whole likelihood fo the tree
        for(int i = 0; i < t_nodes.size(); i++){
                t_nodes[i]->updateResiduals(data, curr_res);
        }

        // cout << "Error C1" << endl;
        // Updating the loglikelihood of the selected pruned node
        p_node->updateResiduals(data,curr_res);
        p_node->nodeLogLike(data, curr_res);
        p_node->left->nodeLogLike(data,curr_res);
        p_node->right->nodeLogLike(data,curr_res);

        // Getting the loglikelihood of the new tree
        double new_tree_log_like =  p_node->log_likelihood - (p_node->left->log_likelihood + p_node->right->log_likelihood);

        // Calculating the transition loglikelihood
        double transition_loglike = log((0.3)/(t_nodes.size())) - log((0.3)/(nog_nodes.size()));

        // Calculating the prior term for the grow
        double tree_prior = log(1-data.alpha*pow((1+p_node->depth),-data.beta))-
                log(data.alpha*pow((1+p_node->depth),-data.beta)) -
                log(1-data.alpha*pow((1+p_node->depth+1),-data.beta)) - // Prior of left node being terminal
                log(1-data.alpha*pow((1+p_node->depth+1),-data.beta));  // Prior of the right noide being terminal
                 // Old current node being terminal


        // Calculating the acceptance
        double acceptance = exp(new_tree_log_like  + transition_loglike + tree_prior);

        if(arma::randu(arma::distr_param(0.0,1.0))<acceptance){
                p_node->deletingLeaves();
                data.move_acceptance(2)++;
        } else {
                // p_node->left->gpNodeLogLike(data, curr_res);
                // p_node->right->gpNodeLogLike(data, curr_res);
        }

        return;
}


// // Creating the change verb
void change(Node* tree, modelParam &data, arma::vec &curr_res){


        // Getting the number of terminal nodes
        std::vector<Node*> t_nodes = leaves(tree) ;
        std::vector<Node*> nog_nodes = nogs(tree);

        // Selecting one node to be sampled
        Node* c_node = sample_node(nog_nodes);

        // In the case of an empty node
        if(c_node->n_leaf==0){
                return;
                c_node->left->updateResiduals(data,curr_res);
                c_node->right->updateResiduals(data,curr_res);
        }

        if(c_node->isRoot){
                // cout << " THAT NEVER HAPPENS" << endl;
               c_node-> n_leaf = data.x_train.n_rows;
               c_node-> n_leaf_test = data.x_test.n_rows;
        }

        // cout << " Change error on terminal nodes" << endl;
        // Calculating the whole likelihood fo the tree
        for(int i = 0; i < t_nodes.size(); i++){
                // cout << "Loglike error " << ed
                t_nodes[i]->updateResiduals(data, curr_res);
        }

        // Calculating the loglikelihood of the old nodes
        c_node->left->nodeLogLike(data,curr_res);
        c_node->right->nodeLogLike(data,curr_res);


        // Storing all the old loglikelihood from left
        double old_left_log_like = c_node->left->log_likelihood;
        double old_left_r_sq_sum = c_node->left->r_sq_sum;
        double old_left_r_sum = c_node->left->r_sum;

        arma::vec old_left_train_index = c_node->left->train_index;
        c_node->left->train_index.fill(-1); // Returning to the original
        int old_left_n_leaf = c_node->left->n_leaf;


        // Storing all of the old loglikelihood from right;
        double old_right_log_like = c_node->right->log_likelihood;
        double old_right_r_sq_sum = c_node->right->r_sq_sum;
        double old_right_r_sum = c_node->right->r_sum;

        arma::vec old_right_train_index = c_node->right->train_index;
        c_node->right->train_index.fill(-1);
        int old_right_n_leaf = c_node->right->n_leaf;



        // Storing test observations
        arma::vec old_left_test_index = c_node->left->test_index;
        arma::vec old_right_test_index = c_node->right->test_index;
        c_node->left->test_index.fill(-1);
        c_node->right->test_index.fill(-1);

        int old_left_n_leaf_test = c_node->left->n_leaf_test;
        int old_right_n_leaf_test = c_node->right->n_leaf_test;


        // Storing the old ones
        int old_var_split = c_node->var_split;
        int old_var_split_rule = c_node->var_split_rule;
        int old_lower = c_node->lower;
        int old_upper = c_node->upper;

        // Choosing only valid cutpoints;
        bool no_valid_node = false;
        int p_try = 0;

        // Trying to find a cutpoint
        arma::vec split_candidates = arma::shuffle(arma::regspace(0,1,data.x_train.n_cols-1));
        Rcpp::NumericVector valid_cutpoint;

        while(!no_valid_node){
                c_node->var_split = split_candidates(p_try);

                Rcpp::NumericVector var_split_range;

                // Getting the maximum and the minimum;
                for(int i = 0; i < c_node->n_leaf; i++){
                        var_split_range.push_back(data.x_train(c_node->train_index[i],c_node->var_split));
                }

                // Getting the minimum and the maximum;
                double max_rule = max(var_split_range);
                double min_rule = min(var_split_range);

                for(int cut = 0; cut < data.xcut.n_rows; cut++ ){
                        if((data.xcut(cut,c_node->var_split)>min_rule) & (data.xcut(cut,c_node->var_split)<max_rule)){
                                valid_cutpoint.push_back(data.xcut(cut,c_node->var_split));
                        }
                }

                if(valid_cutpoint.size()==0){
                        p_try++;
                        if(p_try>=data.x_train.n_cols){
                                no_valid_node = true;
                        };
                } else {
                        break; // Go out from the while
                }
        }

        if(no_valid_node){
                // Returning to the old values
                c_node->var_split = old_var_split;
                c_node->var_split_rule = old_var_split_rule;
                return;
        }

        // Selecting a rule (here I'm actually selecting the var split rule);
        c_node->var_split_rule = valid_cutpoint[arma::randi(arma::distr_param(0,valid_cutpoint.size()))];

        // Create an aux for the left and right index
        int train_left_counter = 0;
        int train_right_counter = 0;

        int test_left_counter = 0;
        int test_right_counter = 0;


        // Updating the left and the right nodes
        for(int i = 0;i<data.x_train.n_rows;i++){
                // cout << " Train indexeses " << c_node -> train_index[i] << endl ;
                if(c_node -> train_index[i] == -1){
                        c_node->left->n_leaf = train_left_counter;
                        c_node->right->n_leaf = train_right_counter;
                        break;
                }
                // cout << " Current train index " << c_node->train_index[i] << endl;

                if(data.x_train(c_node->train_index[i],c_node->var_split)<=c_node->var_split_rule){
                        c_node->left->train_index[train_left_counter] = c_node->train_index[i];
                        train_left_counter++;
                } else {
                        c_node->right->train_index[train_right_counter] = c_node->train_index[i];
                        train_right_counter++;
                }
        }



        // Updating the left and the right nodes
        for(int i = 0;i<data.x_test.n_rows;i++){

                if(c_node -> test_index[i] == -1){
                        c_node->left->n_leaf_test = test_left_counter;
                        c_node->right->n_leaf_test = test_right_counter;
                        break;
                }

                if(data.x_test(c_node->test_index[i],c_node->var_split)<=c_node->var_split_rule){
                        c_node->left->test_index[test_left_counter] = c_node->test_index[i];
                        test_left_counter++;
                } else {
                        c_node->right->test_index[test_right_counter] = c_node->test_index[i];
                        test_right_counter++;
                }
        }

        // If is a root node
        if(c_node->isRoot){
                c_node->left->n_leaf = train_left_counter;
                c_node->right->n_leaf = train_right_counter;
                c_node->left->n_leaf_test = test_left_counter;
                c_node->right->n_leaf_test = test_right_counter;
        }


        if((c_node->left->n_leaf<data.node_min_size) || (c_node->right->n_leaf)<data.node_min_size){

                // Returning to the previous values
                c_node->var_split = old_var_split;
                c_node->var_split_rule = old_var_split_rule;
                c_node->lower = old_lower;
                c_node->upper = old_upper;

                // Returning to the old ones
                c_node->left->r_sq_sum = old_left_r_sq_sum;
                c_node->left->r_sum = old_left_r_sum;

                c_node->left->n_leaf = old_left_n_leaf;
                c_node->left->n_leaf_test = old_left_n_leaf_test;
                c_node->left->log_likelihood = old_left_log_like;
                c_node->left->train_index = old_left_train_index;
                c_node->left->test_index = old_left_test_index;

                // Returning to the old ones
                c_node->right->r_sq_sum = old_right_r_sq_sum;
                c_node->right->r_sum = old_right_r_sum;

                c_node->right->n_leaf = old_right_n_leaf;
                c_node->right->n_leaf_test = old_right_n_leaf_test;
                c_node->right->log_likelihood = old_right_log_like;
                c_node->right->train_index = old_right_train_index;
                c_node->right->test_index = old_right_test_index;

                return;
        }

        // Updating the new left and right loglikelihoods
        c_node->left->updateResiduals(data,curr_res);
        c_node->right->updateResiduals(data,curr_res);
        c_node->left->nodeLogLike(data,curr_res);
        c_node->right->nodeLogLike(data,curr_res);

        // Calculating the acceptance
        double new_tree_log_like =  - old_left_log_like - old_right_log_like + c_node->left->log_likelihood + c_node->right->log_likelihood;

        double acceptance = exp(new_tree_log_like);

        if(arma::randu(arma::distr_param(0.0,1.0))<acceptance){
                // Keep all the trees
                data.move_acceptance(3)++;
        } else {

                // Returning to the previous values
                c_node->var_split = old_var_split;
                c_node->var_split_rule = old_var_split_rule;
                c_node->lower = old_lower;
                c_node->upper = old_upper;

                // Returning to the old ones
                c_node->left->r_sq_sum = old_left_r_sq_sum;
                c_node->left->r_sum = old_left_r_sum;


                c_node->left->n_leaf = old_left_n_leaf;
                c_node->left->n_leaf_test = old_left_n_leaf_test;
                c_node->left->log_likelihood = old_left_log_like;
                c_node->left->train_index = old_left_train_index;
                c_node->left->test_index = old_left_test_index;

                // Returning to the old ones
                c_node->right->r_sq_sum = old_right_r_sq_sum;
                c_node->right->r_sum = old_right_r_sum;

                c_node->right->n_leaf = old_right_n_leaf;
                c_node->right->n_leaf_test = old_right_n_leaf_test;
                c_node->right->log_likelihood = old_right_log_like;
                c_node->right->train_index = old_right_train_index;
                c_node->right->test_index = old_right_test_index;

        }

        return;
}




// Calculating the Loglilelihood of a node
void Node::updateResiduals(modelParam& data, arma::vec &curr_res){

        // Getting number of leaves in case of a root
        if(isRoot){
                // Updating the r_sum
                n_leaf = data.x_train.n_rows;
                n_leaf_test = data.x_test.n_rows;
        }


        // Case of an empty node
        if(train_index[0]==-1){
        // if(n_leaf < 100){
                n_leaf = 0;
                r_sum = 0;
                r_sq_sum =  0;
                log_likelihood = -2000000; // Absurd value avoid this case
                // cout << "OOOPS something happened" << endl;
                return;
        }

        // If is smaller then the node size still need to update the quantities;
        // cout << "Node min size: " << data.node_min_size << endl;
        if(n_leaf < data.node_min_size){
                // log_likelihood = -2000000; // Absurd value avoid this case
                // cout << "OOOPS something happened" << endl;
                return;
        }

        r_sum = 0.0;
        r_sq_sum = 0.0;

        // Train elements
        for(int i = 0; i < n_leaf;i++){
                r_sum = r_sum + curr_res(train_index[i]);
        }

        return;

}

void Node::nodeLogLike(modelParam& data, arma::vec &curr_res){
        // Getting the log-likelihood;
        // log_likelihood = -0.5*data.tau*r_sq_sum - 0.5*log(data.tau_mu + (n_leaf*data.tau)) + (0.5*(data.tau*data.tau)*(r_sum*r_sum))/( (data.tau*n_leaf)+data.tau_mu);
        log_likelihood = - 0.5*log(data.tau_mu + (n_leaf*data.tau)) + (0.5*(data.tau*data.tau)*(r_sum*r_sum))/( (data.tau*n_leaf)+data.tau_mu);
        return;
}

// UPDATING MU
void updateMu(Node* tree, modelParam &data){

        // Getting the terminal nodes
        std::vector<Node*> t_nodes = leaves(tree);

        // Iterating over the terminal nodes and updating the beta values
        for(int i = 0; i < t_nodes.size();i++){
                t_nodes[i]->mu = R::rnorm((data.tau*t_nodes[i]->r_sum)/(t_nodes[i]->n_leaf*data.tau+data.tau_mu),sqrt(1/(data.tau*t_nodes[i]->n_leaf+data.tau_mu))) ;

        }
}




// Get the prediction
// (MOST IMPORTANT AND COSTFUL FUNCTION FROM GP-BART)
void getPredictions(Node* tree,
                    modelParam &data,
                    arma::vec& current_prediction_train,
                    arma::vec& current_prediction_test){

        // Getting the current prediction
        vector<Node*> t_nodes = leaves(tree);
        for(int i = 0; i<t_nodes.size();i++){

                // Skipping empty nodes
                if(t_nodes[i]->n_leaf==0){
                        cout << " THERE ARE EMPTY NODES" << endl;
                        continue;
                }


                // For the training samples
                for(int j = 0; j<data.x_train.n_rows; j++){

                        if((t_nodes[i]->train_index[j])==-1){
                                break;
                        }
                        current_prediction_train[t_nodes[i]->train_index[j]] = t_nodes[i]->mu;
                }

                if(t_nodes[i]->n_leaf_test == 0 ){
                        continue;
                }



                // Regarding the test samples
                for(int j = 0; j< data.x_test.n_rows;j++){

                        if(t_nodes[i]->test_index[j]==-1){
                                break;
                        }

                        current_prediction_test[t_nodes[i]->test_index[j]] = t_nodes[i]->mu;
                }

        }
}


// Updating the tau parameter
void updateTau(arma::vec &y_hat,
               modelParam &data){

        // Getting the sum of residuals square
        double tau_res_sq_sum = dot((y_hat-data.y),(y_hat-data.y));

        data.tau = R::rgamma((0.5*data.y.size()+data.a_tau),1/(0.5*tau_res_sq_sum+data.d_tau));

        return;
}


// Creating the BART function
// [[Rcpp::export]]
Rcpp::List cppbart(arma::mat x_train,
          arma::vec y_train,
          arma::mat x_test,
          arma::mat x_cut,
          int n_tree,
          int node_min_size,
          int n_mcmc,
          int n_burn,
          double tau, double mu,
          double tau_mu,
          double alpha, double beta,
          double a_tau, double d_tau,
          bool stump,
          bool no_rotation_bool){

        // Posterior counter
        int curr = 0;


        // cout << " Error on model.param" << endl;
        // Creating the structu object
        modelParam data(x_train,
                        y_train,
                        x_test,
                        x_cut,
                        n_tree,
                        node_min_size,
                        alpha,
                        beta,
                        tau_mu,
                        tau,
                        a_tau,
                        d_tau,
                        n_mcmc,
                        n_burn,
                        stump);

        // Getting the n_post
        int n_post = n_mcmc - n_burn;

        // Defining those elements
        arma::mat y_train_hat_post = arma::zeros<arma::mat>(data.x_train.n_rows,n_post);
        arma::mat y_test_hat_post = arma::zeros<arma::mat>(data.x_test.n_rows,n_post);

        arma::cube all_tree_post(y_train.size(),n_tree,n_post,arma::fill::zeros);
        arma::vec tau_post = arma::zeros<arma::vec>(n_post);
        arma::vec all_tau_post = arma::zeros<arma::vec>(n_mcmc);


        // Defining other variables
        arma::vec partial_pred = (data.y)/n_tree;
        // arma::vec partial_pred = arma::vec(data.x_train.n_rows,arma::fill::zeros);

        arma::vec partial_residuals = arma::zeros<arma::vec>(data.x_train.n_rows);
        arma::mat tree_fits_store(data.x_train.n_rows,data.n_tree,arma::fill::zeros);
        // for(int i = 0 ; i < data.n_tree ; i ++ ){
        //         tree_fits_store.col(i) = partial_pred;
        // }
        arma::mat tree_fits_store_test(data.x_test.n_rows,data.n_tree,arma::fill::zeros);
        double verb;

        // Defining progress bars parameters
        const int width = 70;
        double pb = 0;


        // cout << " Error one " << endl;

        // Selecting the train
        Forest all_forest(data);

        for(int i = 0;i<data.n_mcmc;i++){

                // Initialising PB
                std::cout << "[";
                int k = 0;
                // Evaluating progress bar
                for(;k<=pb*width/data.n_mcmc;k++){
                        std::cout << "=";
                }

                for(; k < width;k++){
                        std:: cout << " ";
                }

                std::cout << "] " << std::setprecision(5) << (pb/data.n_mcmc)*100 << "%\r";
                std::cout.flush();


                // Getting zeros
                arma::vec prediction_train_sum(data.x_train.n_rows,arma::fill::zeros);
                arma::vec prediction_test_sum(data.x_test.n_rows,arma::fill::zeros);


                for(int t = 0; t<data.n_tree;t++){

                        // Creating the auxliar prediction vector
                        arma::vec y_hat(data.y.n_rows,arma::fill::zeros);
                        arma::vec prediction_test(data.x_test.n_rows,arma::fill::zeros);
                        arma::vec y_hat_var(data.y.n_rows,arma::fill::zeros);
                        arma::vec y_hat_test_var(data.x_test.n_rows,arma::fill::zeros);

                        // cout << "Residuals error "<< endl;
                        // Updating the partial residuals
                        if(data.n_tree>1){
                                partial_residuals = data.y-sum_exclude_col(tree_fits_store,t);

                        } else {
                                partial_residuals = data.y;
                        }

                        // Iterating over all trees
                        verb = arma::randu(arma::distr_param(0.0,1.0));
                        if(all_forest.trees[t]->isLeaf & all_forest.trees[t]->isRoot){
                                // verb = arma::randu(arma::distr_param(0.0,0.3));
                                verb = 0.1;
                        }


                        // Selecting the verb
                        if(verb < 0.25){
                                data.move_proposal(0)++;
                                // cout << " Grow error" << endl;
                                grow(all_forest.trees[t],data,partial_residuals);
                        } else if(verb>=0.25 & verb <0.5) {
                                data.move_proposal(2)++;

                                // cout << " Prune error" << endl;
                                prune(all_forest.trees[t], data, partial_residuals);
                        } else {
                                data.move_proposal(3)++;
                                // cout << " Change error" << endl;
                                change(all_forest.trees[t], data, partial_residuals);
                                // std::cout << "Error after change" << endl;
                        }


                        updateMu(all_forest.trees[t],data);

                        // Getting predictions
                        // cout << " Error on Get Predictions" << endl;
                        getPredictions(all_forest.trees[t],data,y_hat,prediction_test);

                        // Updating the tree
                        // cout << "Residuals error 2.0"<< endl;
                        tree_fits_store.col(t) = y_hat;
                        // cout << "Residuals error 3.0"<< endl;
                        tree_fits_store_test.col(t) = prediction_test;
                        // cout << "Residuals error 4.0"<< endl;


                }

                // Summing over all trees
                prediction_train_sum = sum(tree_fits_store,1);

                prediction_test_sum = sum(tree_fits_store_test,1);


                // std::cout << "Error Tau: " << data.tau<< endl;
                updateTau(prediction_train_sum, data);
                // std::cout << "New Tau: " << data.tau<< endl;
                all_tau_post(i) = data.tau;

                // std::cout << " All good " << endl;
                if(i >= n_burn){
                        // Storing the predictions
                        y_train_hat_post.col(curr) = prediction_train_sum;
                        y_test_hat_post.col(curr) = prediction_test_sum;


                        all_tree_post.slice(curr) = tree_fits_store;
                        tau_post(curr) = data.tau;
                        curr++;
                }

                pb += 1;

        }
        // Initialising PB
        std::cout << "[";
        int k = 0;
        // Evaluating progress bar
        for(;k<=pb*width/data.n_mcmc;k++){
                std::cout << "=";
        }

        for(; k < width;k++){
                std:: cout << " ";
        }

        std::cout << "] " << std::setprecision(5) << 100 << "%\r";
        std::cout.flush();

        std::cout << std::endl;

        return Rcpp::List::create(y_train_hat_post, //[1]
                                  y_test_hat_post, //[2]
                                  tau_post, //[3]
                                  all_tree_post, // [4]
                                  data.move_proposal, // [5]
                                  data.move_acceptance,// [6]
                                  all_tau_post // [7]
                                );
}


