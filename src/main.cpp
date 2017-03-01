#include "ACLsimulatorimpl.h"
//#include "vegafem.h"
#include "addBodies.h"
#include "addKneeContacts.h"
#include "CustomLigament.h"
#include <ctime>

#include <string>
#include <vector>

using namespace OpenSim;
using namespace SimTK;
using namespace std;

int main(int argc, char *argv[])
{
	try {
		Object::registerType(CustomLigament());
		// Create an OpenSim model and set its name
		OpenSim::Model model("../resources/geometries/closed_knee_ligaments_0_3 - post_ant_load_2.osim");

		//// add meniscus bodies to left and right knee
		//cout << "Adding meniscus" << endl;
		//addMeniscusWeldJoints(model, true);
		//addMeniscusWeldJoints(model, false);
		//// add femur bodies and weld joints 
		//cout << "Adding femur" << endl;
		//addFemurWeldJoints(model, true);
		//addFemurWeldJoints(model, false);
		//// add tibia weld joint
		//cout << "Addint tibia upper bodies" << endl;
		////addTibiaWeldJoints(model, true);
		////addTibiaWeldJoints(model, false);
		//addUpperTibiaFreeJoints( model, true);
		//addUpperTibiaFreeJoints( model, false);
		//// add contact geometries at the right knee
		//cout << "Adding contacts" << endl;
		//addKneeContactGeometries(model, true);
		//addKneeContactGeometries(model, false);	
		//// add contact forces
		//cout << "Adding contact forces" << endl;
		//addEFForce(model, 1.E12, 1.0, 0.8, 0.04, 0.04, true);
		//addEFForce(model, 1.E12, 1.0, 0.8, 0.04, 0.04, false);	
		//model.print("../resources/geometries/closed_knee_ligaments_1_0.osim");
		// add patella custom joint
		
		//printLigamentLengths(model);

		 //Get a reference to the model's ground body
		//OpenSim::Body* tibia_upper_r = &model.updBodySet().get("tibia_r");

		//Vec3 blockMassCenter(0);
		//Inertia blockInertia = 0*Inertia::brick(Vec3(0));
		//// Create a new block body with specified properties
		//OpenSim::Body *block1 = new OpenSim::Body("block", 1, blockMassCenter, blockInertia);
		//// Add display geometry to the block to visualize in the GUI
		//block1->addDisplayGeometry("blockMesh.obj");
		//WeldJoint *weldJ = new WeldJoint("block_joint_", *tibia_upper_r, SimTK::Vec3(0.0, -0.05, -0.5), 
		//SimTK::Vec3(0), *block1, SimTK::Vec3(0), SimTK::Vec3(0), true); 
		//model.addBody( block1);		

		//// Create a new block body with specified properties
		//OpenSim::Body *block2 = new OpenSim::Body("block", 1, blockMassCenter, blockInertia);
		//// Add display geometry to the block to visualize in the GUI
		//block2->addDisplayGeometry("blockMesh.obj");
		//WeldJoint *weldJ2 = new WeldJoint("block_joint_", *tibia_upper_r, SimTK::Vec3(0.0, -0.25, 0.5), 
		//SimTK::Vec3(0), *block2, SimTK::Vec3(0), SimTK::Vec3(0), true); 
		//model.addBody( block2);		

		//model.print("../resources/geometries/block.osim");

		// simulate
		//inverseSimulation(model);
		//staticOptimization(model);
		forwardSimulation(model);
		
		 //Save the model to a file
		//model.setUseVisualizer(1);
		//SimTK::State& state = model.initSystem();
		//state = model.initSystem();
		//model.getVisualizer().show(state);

		std::cout << "OpenSim example completed successfully.\n";
		std::cin.get();

		return 0;
	}
	catch (OpenSim::Exception ex)
    {
        std::cout << "OpenSim exception\n" << ex.getMessage() << std::endl;
    }
	catch (SimTK::Exception::ErrorCheck ex)
	{
		cout << "Simbody exception\n" << ex.getMessage() << endl;
	}
    catch (std::exception ex)
    {
		std::cout << "std exception: " << ex.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "UNRECOGNIZED EXCEPTION" << std::endl;
    }

	std::cin.get();
	return 1;
}
