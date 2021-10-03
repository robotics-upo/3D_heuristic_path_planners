#include "Planners/LazyThetaStarGenerator.hpp"

namespace Planners
{
    LazyThetaStarGenerator::LazyThetaStarGenerator(bool _use_3d):ThetaStarGenerator(_use_3d, "lazythetastar") {}
    LazyThetaStarGenerator::LazyThetaStarGenerator(bool _use_3d, std::string _name = "lazythetastar" ):ThetaStarGenerator(_use_3d, _name) {}
    
    void LazyThetaStarGenerator::UpdateVertex(Node *_s, Node *_s2, NodeSet &_openset)
    {
        unsigned int g_old = _s2->G;

        ComputeCost(_s, _s2);
        if (_s2->G < g_old)
        {
            if (discrete_world_.isInOpenList(*_s2))
                _openset.erase(_s2);

            _openset.insert(_s2);
        }
    }
    void LazyThetaStarGenerator::SetVertex(Node *_s_aux)
    {
        line_of_sight_checks_++;

        if (!LineOfSight::bresenham3D((_s_aux->parent), _s_aux, discrete_world_))
        {
            unsigned int G_max = std::numeric_limits<unsigned int>::max(); 
            unsigned int G_new;

            for (const auto &i: direction)
            {
                Vec3i newCoordinates(_s_aux->coordinates + i);

                if ( discrete_world_.isOccupied(newCoordinates) ) continue;

                if ( discrete_world_.isInClosedList(newCoordinates) )
                {
                    Node *successor2 = discrete_world_.getNodePtr(newCoordinates);
                    if (successor2 == nullptr) continue;

                    G_new = successor2->G +  geometry::distanceBetween2Nodes(successor2, _s_aux);
                    if (G_new < G_max)
                    {
                        G_max = G_new;
                        _s_aux->parent = successor2;
                        _s_aux->G = G_new;
                    }
                }
            }
        }
    }
    void LazyThetaStarGenerator::ComputeCost(Node *_s_aux, Node *_s2_aux)
    {
        auto distanceParent2 = geometry::distanceBetween2Nodes(_s_aux->parent, _s2_aux);

        if ((_s_aux->parent->G + distanceParent2) < (_s2_aux->G))
        {
            _s2_aux->parent = _s_aux->parent;
            _s2_aux->G = _s2_aux->parent->G + distanceParent2;
        }
    }

    PathData LazyThetaStarGenerator::findPath(const Vec3i &_source, const Vec3i &_target)
    {
        Node *current = nullptr;
        NodeSet openSet, closedSet;
        bool solved{false};

        openSet.insert(discrete_world_.getNodePtr(_source));

        discrete_world_.getNodePtr(_source)->parent = new Node(_source);
        discrete_world_.setOpenValue(_source, true);

        utils::Clock main_timer;
        main_timer.tic();

        line_of_sight_checks_ = 0;

        while (!openSet.empty())
        {

            current = *openSet.begin();

            if (current->coordinates == _target)
            {
                solved = true;
                break;
            }

            openSet.erase(openSet.begin());
            closedSet.insert(current);

            discrete_world_.setOpenValue(*current, false);
            discrete_world_.setClosedValue(*current, true);

            SetVertex(current);
#if defined(ROS) && defined(PUB_EXPLORED_NODES)
            publishROSDebugData(current, openSet, closedSet);
#endif

            for (unsigned int i = 0; i < direction.size(); ++i)
            {

                Vec3i newCoordinates(current->coordinates + direction[i]);

                if (discrete_world_.isOccupied(newCoordinates) ||
                    discrete_world_.isInClosedList(newCoordinates))
                    continue;

                Node *successor = discrete_world_.getNodePtr(newCoordinates);

                if (successor == nullptr) continue;

                if (!discrete_world_.isInOpenList(newCoordinates))
                {
                    unsigned int totalCost = current->G;

                    if(direction.size()  == 8){
                        totalCost += (i < 4 ? dist_scale_factor_ : dd_2D_); //This is more efficient
                    }else{
                        totalCost += (i < 6 ? dist_scale_factor_ : (i < 18 ? dd_2D_ : dd_3D_)); //This is more efficient
                    }

                    successor->parent = current;
                    successor->G = totalCost; 
                    successor->H = heuristic(successor->coordinates, _target);
                    openSet.insert(successor);
                    discrete_world_.setOpenValue(*successor, true);
                }
                UpdateVertex(current, successor, openSet);
            }
        }
        main_timer.toc();
    
        PathData result_data = createResultDataObject(current, main_timer, closedSet.size(), 
                                                  solved, _source, 0);
   
#if defined(ROS) && defined(PUB_EXPLORED_NODES)
        explored_node_marker_.points.clear();
#endif

        discrete_world_.resetWorld();
        return result_data;
    }

}
