#include "ACLsimulatorimpl.h"
#include "osimutils.h"
#include <ctime>
#include "CustomLigament.h"
#include <OpenSim/Auxiliary/auxiliaryTestFunctions.h>


template<typename T> std::string changeToString(
    const T& value, int precision = std::numeric_limits<int>::infinity())
{
    std::ostringstream oss;
    if (precision != std::numeric_limits<int>::infinity())
    {
        oss << std::setprecision(precision);
    }     oss << value;     return oss.str();
}

void forwardSim(Model model){
    model.setGravity(Vec3(0,-9.9,0));

	// name parameters needed
	Vector activs_initial, activs_final;		//	activations for the current position and activations for the next position
	vector<OpenSim::Function*> controlFuncs;	//	control functions for every t (time)
	vector<Vector> acts;						//	activations
	vector<double> actsTimes;					//	every t (=time) when I have an activation to apply
	double duration = 0.5;

	SimTK::State& state = model.initSystem();

	// compute muscle activations for specific knee angle (in rads)
	double knee_angle = -1.0;
	computeActivations(model, knee_angle, controlFuncs, duration, true, activs_initial, activs_final, state);

	// add controller to the model after adding the control functions to the controller
	KneeController *knee_controller = new KneeController( controlFuncs.size());
    knee_controller->setControlFunctions(controlFuncs);
	knee_controller->setActuators(model.getActuators());
	model.addController(knee_controller);

	// set model to initial state
	state = model.initSystem();
    //model.equilibrateMuscles(state);

    // Add reporters
    ForceReporter* forceReporter = new ForceReporter(&model);
    model.addAnalysis(forceReporter);

	// Simulate
    RungeKuttaMersonIntegrator integrator(model.getMultibodySystem());
    Manager manager(model, integrator);
    manager.setInitialTime(0);
    manager.setFinalTime(2);

	time_t result = std::time(nullptr);
	std::cout << "\nBefore integrate(si) " << std::asctime(std::localtime(&result)) << endl;
	
    manager.integrate(state);

	result = std::time(nullptr);
	std::cout << "\nAfter integrate(si) " << std::asctime(std::localtime(&result)) << endl;

	// Save the simulation results
	Storage statesDegrees(manager.getStateStorage());
	statesDegrees.print("../outputs/kneeforwsim_states.sto");
	model.updSimbodyEngine().convertRadiansToDegrees(statesDegrees);
	statesDegrees.setWriteSIMMHeader(true);
	statesDegrees.print("../outputs/kneeforwsim_states_degrees.mot");

	// Get activation trajectory
    acts.resize(knee_controller->getControlLog().size());
    actsTimes.resize(knee_controller->getControlLog().size());

    vector<int> index(acts.size(), 0);
    for (int i = 0 ; i != index.size() ; i++) {
      index[i] = i;
    }

    sort(index.begin(), index.end(),
         [&](const int& a, const int& b) {
           return (knee_controller->getControlTimesLog()[a] <
                   knee_controller->getControlTimesLog()[b]);
         }
    );

    for (int i = 0 ; i != acts.size() ; i++) {
        acts[i] = knee_controller->getControlLog()[index[i]];
        actsTimes[i] = knee_controller->getControlTimesLog()[index[i]];
    }

    // Delete duplicates
    int ii = 0;
    for (unsigned i=1; i<acts.size(); i++) {
        if (actsTimes[i] <= actsTimes[ii]) {
            actsTimes.erase(actsTimes.begin()+i);
            acts.erase(acts.begin()+i);
            --i;
        }
        ii = i;
    }

	OsimUtils::writeFunctionsToFile( actsTimes, acts, "../outputs/force_Excitations_LOG.sto");
}

void computeActivations (Model &model, const double angle, vector<OpenSim::Function*> &controlFuncs,
			double &duration, bool store, Vector &so_activ_init, Vector &so_activ_final, State &si)
{
    calcSSact(model, so_activ_init, si);

	//set movement parameters
	const CoordinateSet &knee_r_cs = model.getJointSet().get("knee_r").getCoordinateSet();
    knee_r_cs.get("knee_angle_r").setValue(si, angle);
	model.equilibrateMuscles(si);

	calcSSact(model, so_activ_final, si);

    const int N = 9;

    const double t_eq = 0.000;
    const double dur_xc = (25.0 + 0.2 * angle) / 1000.;
    const double t_fix  = (99.0 + 0.5 * angle) / 1000.;

    double phases[N] = {
       -t_eq,
        0,
        0,
        dur_xc,
        dur_xc * 1.05,
        t_fix,
        t_fix + 0.005,
        t_fix + 0.010,
        t_fix*2
    };
    for (int i=0; i<N; i++) phases[i]+=t_eq;

    // Construct control functions
    controlFuncs.clear();
    double values[N];

    for (int i = 0; i < so_activ_final.size(); i++)  {
        double dssa = so_activ_final[i] - so_activ_init[i];
        int j=0;

        values[j++] = so_activ_init[i];
        values[j++] = values[j-1];

        values[j++] = so_activ_final[i] + dssa * 0.75;
        values[j++] = values[j-1];

        values[j++] = so_activ_init[i] + dssa * 0.975;
        values[j++] = values[j-1];

        values[j++] = so_activ_init[i] + dssa * 1.350;

        values[j++] = so_activ_final[i];
        values[j++] = values[j-1];

        OpenSim::Function *controlFunc = new PiecewiseLinearFunction(N, phases, values);
        controlFunc->setName(model.getActuators()[i].getName());
        controlFunc->setName("Excitation_" + model.getActuators()[i].getName());
        controlFuncs.push_back(controlFunc);
    }

    duration = t_eq + t_fix * 2;

	if (store == true) OsimUtils::writeFunctionsToFile( controlFuncs, "../outputs/_Excitations_LOG.sto", duration, 0.001 );
}

void calcSSact(Model &model, Vector &activations, State &si)
{
	// Perform a dummy forward simulation without forces,
    // just to obtain a state-series to be used by stat opt
    OsimUtils::disableAllForces(si, model);
	// Create the integrator and manager for the simulation.
    SimTK::RungeKuttaMersonIntegrator integrator( model.getMultibodySystem() );
    Manager manager( model, integrator );
	// Integrate from initial time to final time.
    manager.setInitialTime( 0);
    manager.setFinalTime( 2);
    std::cout << "\n\nIntegrating from 0 to 2 " << std::endl;
    manager.integrate( si);

    // Perform a quick static optimization that will give us
    // the steady state activations needed to overcome the passive forces
    OsimUtils::enableAllForces(si, model);

    Storage &states = manager.getStateStorage();
    states.setInDegrees(false);
    StaticOptimization so(&model);
    so.setStatesStore(states);
    State &s = model.initSystem();

    states.getData(0, s.getNY(), &s.updY()[0]);
    s.setTime(0);
    so.begin(s);
    so.end(s);

    Storage *as = so.getActivationStorage();
    int na = model.getActuators().getSize();
    activations.resize(na);
    int row = as->getSize() - 1;

    // Store activations to out vector
    for (int i = 0; i<na; i++)
	{
        as->getData(row, i, activations[i]);
		//// print results
		//cout << as->getColumnLabels()[i+1] << ": " << activations[i] << endl;
		//cout << as->getDataColumn() << "\n" << endl;
	}
}

void inverseSimulation(Model model)
{
	Array_<Real> times;
	double dur = 1.0;
	double step = 0.001;

	// Prepare time series
    InverseDynamicsSolver ids(model);
    times.resize((int)(dur / step) + 1, 0);
    for (unsigned i = 0; i < times.size(); i++)
        times[i] = step * i;

	// Solve for generalized joints forces
    State &s = model.initSystem();
	Vector ids_results = ids.solve(s, SimTK::Vector(0));

	for (int i=0; i<ids_results.size(); i++)
		cout << i << " : " << ids_results(i) << endl;

	//for (int j=0; j<s.getQ().size(); j++)
	//{
	//	cout << "Q(" << j << "): " << s.getQ()[j] << endl;
	//}
	//
	//Array<std::string> stateNames = model.getStateVariableNames();    
	//printf("print state variable names (%d):\n", stateNames.size());
	//for (int j=0; j<stateNames.size(); j++)
 //   {
 //       printf("state variable name %d: %s\n", j, stateNames[j].c_str());
	//}
}

void staticOptimization(Model model)
{
    // set gravity off    
    //model.setGravity(Vec3(0,0,0));

    SimTK::State& state = model.initSystem();
    std::vector<std::vector<double>> acts;                // activations
    vector<double> actsTimes;                   // states.size()                      
    // Create the state sequence of motion (knee flexion)
    Storage states;
    states.setDescription("Knee flexion");
    Array<std::string> stateNames = model.getStateVariableNames();    
    stateNames.insert(0, "time");
    states.setColumnLabels(stateNames);
    Array<double> stateVals;

    const CoordinateSet &knee_r_cs = model.getJointSet().get("knee_r").getCoordinateSet();
    double knee_angle_r = -0.0290726;
    double t = 0.0;
    for (int i=0; i<20; i++)
    {
        knee_angle_r = -1.0/20 + knee_angle_r;
        knee_r_cs.get("knee_angle_r").setValue(state, knee_angle_r);
        model.getStateValues(state,stateVals);
        states.append( t, stateVals.size(), stateVals.get());
        t = t + 0.1;
    }
    t = t - 0.1;
    actsTimes.resize(states.getSize());
    states.setInDegrees(false);
    
	// Perform the static optimization
    StaticOptimization so(&model);
    so.setStatesStore(states);
    int ns = states.getSize(); 
    double ti, tf;
    states.getTime(0, ti);
    states.getTime(ns-1, tf);
    so.setStartTime(ti);
    so.setEndTime(tf);

    // Run the analysis loop
    state = model.initSystem();
    for (int i = 0; i < ns; i++) {
        states.getData(i, state.getNY(), &state.updY()[0]);
        Real t = 0.0;
        states.getTime(i, t);
        state.setTime(t);
        model.assemble(state);
        model.getMultibodySystem().realize(state, SimTK::Stage::Velocity);

        if (i == 0 )
          so.begin(state);
        else if (i == ns)
          so.end(state);
        else
          so.step(state, i);
    }

	// store .mot file
	Storage* activations = so.getActivationStorage();
	activations->print("../outputs/so_acts.sto");
	Storage* forces = so.getForceStorage();
	forces->print("../outputs/so_forces.sto");

	//cout << forces->getName() << endl;
}

void anteriorTibialLoadsFD(Model& model)
{
	// add external force
	//addExternalForce(model, -0.05, -0.5);
	//addExternalForce(model, -0.05, 0.5);
	//addExternalForce(model, -0.1, -0.5);   
	//addExternalForce(model, -0.1, 0.5);   
	double knee_angle = -90;
	addTibialLoads(model, knee_angle);
	
	// init system
	std::time_t result = std::time(nullptr);
	std::cout << "\nBefore initSystem() " << std::asctime(std::localtime(&result)) << endl;
	SimTK::State& si = model.initSystem();
	result = std::time(nullptr);
	std::cout << "\nAfter initSystem() " << std::asctime(std::localtime(&result)) << endl;
	
	// set gravity	
	model.updGravityForce().setGravityVector(si, Vec3(0,0,0));

	// disable muscles
	string muscle_name;
	//for (int i=0; i<model.getActuators().getSize(); i++)
	//{
		//muscle_name = model.getActuators().get(i).getName();

		//model.getActuators().get(i).setDisabled(si, true);

		//if (muscle_name == "bifemlh_r" || muscle_name == "bifemsh_r" || muscle_name == "grac_r" \
		//	|| muscle_name == "lat_gas_r" || muscle_name == "med_gas_r" || muscle_name == "sar_r" \
		//	|| muscle_name == "semimem_r" || muscle_name == "semiten_r" \
		//	|| muscle_name == "rect_fem_r" || muscle_name == "vas_med_r" || muscle_name == "vas_int_r" || muscle_name == "vas_lat_r" )
		//		model.getActuators().get(i).setDisabled(si, false);
	//}

	setKneeAngle(model, si, knee_angle);
	model.equilibrateMuscles( si);

	// Add reporters
    ForceReporter* forceReporter = new ForceReporter(&model);
    model.addAnalysis(forceReporter);

	CustomAnalysis* customReporter = new CustomAnalysis(&model, "r");
	model.addAnalysis(customReporter);

	// Create the integrator and manager for the simulation.
	SimTK::RungeKuttaMersonIntegrator integrator(model.getMultibodySystem());
	//integrator.setAccuracy(1.0e-3);
	//integrator.setFixedStepSize(0.001);
	Manager manager(model, integrator);

	// Define the initial and final simulation times
	double initialTime = 0.0;
	double finalTime = 1.0;

	// Integrate from initial time to final time
	manager.setInitialTime(initialTime);
	manager.setFinalTime(finalTime);
	std::cout<<"\n\nIntegrating from "<<initialTime<<" to " <<finalTime<<std::endl;

	result = std::time(nullptr);
	std::cout << "\nBefore integrate(si) " << std::asctime(std::localtime(&result)) << endl;

	manager.integrate(si);

	result = std::time(nullptr);
	std::cout << "\nAfter integrate(si) " << std::asctime(std::localtime(&result)) << endl;

	// Save the simulation results
	//osimModel.updAnalysisSet().adoptAndAppend(forces);
	Storage statesDegrees(manager.getStateStorage());
	statesDegrees.print("../outputs/states_ant_load_" + changeToString(knee_angle) +".sto");
	model.updSimbodyEngine().convertRadiansToDegrees(statesDegrees);
	statesDegrees.setWriteSIMMHeader(true);
	statesDegrees.print("../outputs/states_degrees_ant_load_" + changeToString(knee_angle) +".mot");
	// force reporter results
	model.updAnalysisSet().adoptAndAppend(forceReporter);
	forceReporter->getForceStorage().print("../outputs/force_reporter_ant_load_" + changeToString(knee_angle) +".mot");
	customReporter->print( "../outputs/custom_reporter_ant_load_" + changeToString(knee_angle) +".mot");
}

void forwardSimulation(Model& model)
{
	addFlexionController(model);
	//addExtensionController(model);

	// init system
	std::time_t result = std::time(nullptr);
	std::cout << "\nBefore initSystem() " << std::asctime(std::localtime(&result)) << endl;
	SimTK::State& si = model.initSystem();
	result = std::time(nullptr);
	std::cout << "\nAfter initSystem() " << std::asctime(std::localtime(&result)) << endl;
	
	// set gravity
	model.updGravityForce().setGravityVector(si, Vec3(-9.80665,0,0));
	//model.updGravityForce().setGravityVector(si, Vec3(0,0,0));
	
	// disable muscles
	//string muscle_name;
	//for (int i=0; i<model.getActuators().getSize(); i++)
	//{
	//	muscle_name = model.getActuators().get(i).getName();

	//	model.getActuators().get(i).setDisabled(si, true);

	//	if (muscle_name == "bifemlh_r" || muscle_name == "bifemsh_r" || muscle_name == "grac_r" \
	//		|| muscle_name == "lat_gas_r" || muscle_name == "med_gas_r" || muscle_name == "sar_r" \
	//		|| muscle_name == "semimem_r" || muscle_name == "semiten_r" \
	//		|| muscle_name == "rect_fem_r" || muscle_name == "vas_med_r" || muscle_name == "vas_int_r" || muscle_name == "vas_lat_r" )
	//			model.getActuators().get(i).setDisabled(si, false);
	//}

	//setKneeAngle(model, si, 0);
	model.equilibrateMuscles( si);

	// Add reporters
    ForceReporter* forceReporter = new ForceReporter(&model);
    model.addAnalysis(forceReporter);

	CustomAnalysis* customReporter = new CustomAnalysis(&model, "r");
	model.addAnalysis(customReporter);

	// Create the integrator and manager for the simulation.
	SimTK::RungeKuttaMersonIntegrator integrator(model.getMultibodySystem());
	//integrator.setAccuracy(1.0e-3);
	//integrator.setFixedStepSize(0.001);
	Manager manager(model, integrator);

	// Define the initial and final simulation times
	double initialTime = 0.0;
	double finalTime = 0.2;

	// Integrate from initial time to final time
	manager.setInitialTime(initialTime);
	manager.setFinalTime(finalTime);
	std::cout<<"\n\nIntegrating from "<<initialTime<<" to " <<finalTime<<std::endl;

	result = std::time(nullptr);
	std::cout << "\nBefore integrate(si) " << std::asctime(std::localtime(&result)) << endl;

	manager.integrate(si);

	result = std::time(nullptr);
	std::cout << "\nAfter integrate(si) " << std::asctime(std::localtime(&result)) << endl;

	// Save the simulation results
	Storage statesDegrees(manager.getStateStorage());
	statesDegrees.print("../outputs/states_flex.sto");
	model.updSimbodyEngine().convertRadiansToDegrees(statesDegrees);
	statesDegrees.setWriteSIMMHeader(true);
	statesDegrees.print("../outputs/states_degrees_flex.mot");
	// force reporter results
	forceReporter->getForceStorage().print("../outputs/force_reporter_flex.mot");
	customReporter->print( "../outputs/custom_reporter_flex.mot");
}

void addTibialLoads(Model& model, double knee_angle)
{
	// Specify properties of a force function to be applied to the block
	//double timeX[2] = {0.0, 0.5}; // time nodes for linear function
	//double timeY[2] = {0.0, 0.5}; // time nodes for linear function
	//double fXofT[2] = {0, -103.366188 / 4.0f}; // force values at t1 and t2
	//double fYofT[2] = {0, 37.6222 / 4.0f}; // force values at t1 and t2
	//double pYofT[2] = {0, -0.5}; // point values at t1 and t2
 
	//// Create a new linear functions for the force and point components
	//PiecewiseLinearFunction *forceX = new PiecewiseLinearFunction(2, timeX, fXofT);
	//PiecewiseLinearFunction *forceY = new PiecewiseLinearFunction(2, timeY, fYofT);
	//PiecewiseLinearFunction *pointY = new PiecewiseLinearFunction(2, timeY, pYofT);
  
	// Create a new prescribed force applied to the block
	//PrescribedForce *prescribedF = new PrescribedForce();
	//OpenSim::Body* tibia_body = &model.updBodySet().get("tibia_r");
	PrescribedForce *prescribedForce = new PrescribedForce();
	ostringstream strs;
	strs << "prescribedForce_" << knee_angle;
	prescribedForce->setName(strs.str());
	prescribedForce->setBodyName("tibia_r");

	// Set the force and point functions for the new prescribed force
	if (knee_angle == 0)
		prescribedForce->setForceFunctions(new Constant(110.0), new Constant(0.0), new Constant(0.0));		// at 0 degrees
	else if (knee_angle == -15)
		prescribedForce->setForceFunctions(new Constant(106.25), new Constant(-28.47), new Constant(0.0));	// at -15 degrees
	else if (knee_angle == -20)
		prescribedForce->setForceFunctions(new Constant(103.366188), new Constant(-37.6222), new Constant(0.0));	// at -20 degrees (knee_angle)
	else if (knee_angle == -40)
		prescribedForce->setForceFunctions(new Constant(84.26488), new Constant(-70.7066), new Constant(0.0));	// at -40 degrees (knee_angle)
	else if (knee_angle == -60)
		prescribedForce->setForceFunctions(new Constant(55), new Constant(-95.2627), new Constant(0.0));	// at -60 degrees (knee_angle)
	else if (knee_angle == -80)
		prescribedForce->setForceFunctions(new Constant(19.101), new Constant(-108.3288), new Constant(0.0));	// at -80 degrees (knee_angle)	
	else if (knee_angle == -90)
		prescribedForce->setForceFunctions(new Constant(0), new Constant(-110.0), new Constant(0.0));	// at -90 degrees (knee_angle)	

	//prescribedForce->setPointFunctions(new Constant(0.0), new Constant(const_point_y), new Constant(const_point_z));

	//prescribedForce->setForceIsInGlobalFrame(true);
	//prescribedForce->setPointIsInGlobalFrame(true);

	// Add the new prescribed force to the model
	model.addForce(prescribedForce);
}

void addExternalForce(Model& model, double const_point_y, double const_point_z)
{
	// Specify properties of a force function to be applied to the block
	double timeX[2] = {0.0, 0.5}; // time nodes for linear function
	double timeY[2] = {0.0, 0.5}; // time nodes for linear function
	double fXofT[2] = {0, -103.366188 / 4.0f}; // force values at t1 and t2
	double fYofT[2] = {0, 37.6222 / 4.0f}; // force values at t1 and t2
  
	// Create a new linear functions for the force and point components
	PiecewiseLinearFunction *forceX = new PiecewiseLinearFunction(2, timeX, fXofT);
	PiecewiseLinearFunction *forceY = new PiecewiseLinearFunction(2, timeY, fYofT);
  
	// Create a new prescribed force applied to the block
	//PrescribedForce *prescribedF = new PrescribedForce();
	//OpenSim::Body* tibia_body = &model.updBodySet().get("tibia_r");
	ExternalForce *externalForce = new ExternalForce( Storage("C:/Users/Maria/Documents/GitHub/ACLproj/outputs/sx.xml"), "force", "point", "torque", "tibia_upper_r", "ground", "tibia_upper_r");
	externalForce->setName("externalTibialForce");
	//externalForce->setAppliedToBodyName("tibia_upper_r");
	//externalForce->setPointExpressedInBodyName("ground");
	//externalForce->setForceExpressedInBodyName("tibia_upper_r");
	// Set the force and point functions for the new externalForce

	// Add the new externalForce to the model
	model.addForce(externalForce);
}

void addFlexionController(Model& model)
{
	PrescribedController* controller = new PrescribedController();
	controller->setName( "flexion_controller");
	controller->setActuators( model.updActuators());
	
	double control_time[2] = {0, 0.05}; // time nodes for linear function
	double control_acts[2] = {1.0, 0}; // force values at t1 and t2
	//control_func->setName( "constant_control_func");

	string muscle_name;
	for (int i=0; i<model.getActuators().getSize(); i++)
	{
		muscle_name = model.getActuators().get(i).getName();
		// hamstrings: bi*, semi*
		if ( muscle_name == "bifemlh_r" || muscle_name == "bifemsh_r" || muscle_name == "grac_r" \
			|| muscle_name == "lat_gas_r" || muscle_name == "med_gas_r" || muscle_name == "sar_r" \
			|| muscle_name == "semimem_r" || muscle_name == "semiten_r")
		{
			Constant* ccf = new Constant(1.0);
			//PiecewiseLinearFunction *ccf = new PiecewiseLinearFunction( 2, control_time, control_acts);
			controller->prescribeControlForActuator( i, ccf);
		}
		else 
		{
			Constant* zccf = new Constant(0);
			controller->prescribeControlForActuator( i, zccf);
		}
	}
	model.addController( controller);
}

void addExtensionController(Model& model)
{
	PrescribedController* controller = new PrescribedController();
	controller->setName( "extension_controller");
	controller->setActuators( model.updActuators());
	
	double control_time[2] = {0.01, 0.02}; // time nodes for linear function
	double control_acts[2] = {0.0, 1.0}; // force values at t1 and t2
	//control_func->setName( "constant_control_func");

	string muscle_name;
	for (int i=0; i<model.getActuators().getSize(); i++)
	{
		muscle_name = model.getActuators().get(i).getName();
		// activate quadriceps
		if (muscle_name == "rect_fem_r" || muscle_name == "vas_med_r" || muscle_name == "vas_int_r" || muscle_name == "vas_lat_r" )
		{
			Constant* ccf = new Constant(0.6);
			//PiecewiseLinearFunction *ccf = new PiecewiseLinearFunction( 2, control_time, control_acts);
			controller->prescribeControlForActuator( i, ccf);
		}
		else 
		{
			Constant* zccf = new Constant(0);
			controller->prescribeControlForActuator( i, zccf);
		}
	}
	model.addController( controller);
}

void setKneeAngle(Model& model, SimTK::State &si, double angle_degrees)
{
	const CoordinateSet &knee_r_cs = model.getJointSet().get("knee_r").getCoordinateSet();
	if (angle_degrees == -120)
	{
		knee_r_cs.get("knee_angle_r").setValue(si, -2.09439510);  // -120 degrees
		
		knee_r_cs.get("knee_adduction_r").setValue(si, -0.19163894);
		knee_r_cs.get("knee_rotation_r").setValue(si, 0.02110966);
		knee_r_cs.get("knee_anterior_posterior_r").setValue(si, 0.02843407);
		knee_r_cs.get("knee_inferior_superior_r").setValue(si, -0.41174209);
		knee_r_cs.get("knee_medial_lateral_r").setValue(si, -0.00329063);
	}
	else if (angle_degrees == -100)
	{
		knee_r_cs.get("knee_angle_r").setValue(si, -1.74533);  // -100 degrees
		
		knee_r_cs.get("knee_adduction_r").setValue(si, -0.23053779);
		knee_r_cs.get("knee_rotation_r").setValue(si, 0.00044497);
		knee_r_cs.get("knee_anterior_posterior_r").setValue(si, 0.0293309);
		knee_r_cs.get("knee_inferior_superior_r").setValue(si, -0.40140432);
		knee_r_cs.get("knee_medial_lateral_r").setValue(si, -0.00504724);
	}
	else if (angle_degrees == -90)
	{
		knee_r_cs.get("knee_angle_r").setValue(si, -1.57079);	// -90 degrees

		knee_r_cs.get("knee_adduction_r").setValue(si, -0.24);
		knee_r_cs.get("knee_rotation_r").setValue(si, 0.008);
		knee_r_cs.get("knee_anterior_posterior_r").setValue(si, 0.0275);
		knee_r_cs.get("knee_inferior_superior_r").setValue(si, -0.396);
		knee_r_cs.get("knee_medial_lateral_r").setValue(si, -0.005);
	}
	else if (angle_degrees == -80)
	{
		knee_r_cs.get("knee_angle_r").setValue(si, -1.39626);  // -80 degrees
			
		knee_r_cs.get("knee_adduction_r").setValue(si, -0.24427703);
		knee_r_cs.get("knee_rotation_r").setValue(si, 0.01682137);
		knee_r_cs.get("knee_anterior_posterior_r").setValue(si, 0.02661332);
		knee_r_cs.get("knee_inferior_superior_r").setValue(si, -0.39351699 );
		knee_r_cs.get("knee_medial_lateral_r").setValue(si, -0.00483042);
	}
	else if (angle_degrees == -60)
	{
		knee_r_cs.get("knee_angle_r").setValue(si, -1.0472);  // -60 degrees
			
		knee_r_cs.get("knee_adduction_r").setValue(si, -0.29941123);
		knee_r_cs.get("knee_rotation_r").setValue(si, -0.00183259);
		knee_r_cs.get("knee_anterior_posterior_r").setValue(si, 0.02092232);
		knee_r_cs.get("knee_inferior_superior_r").setValue(si, -0.38597298);
		knee_r_cs.get("knee_medial_lateral_r").setValue(si, -0.00403978);
	}
	else if (angle_degrees == -40)
	{
		knee_r_cs.get("knee_angle_r").setValue(si, -0.698132);  // -40 degrees
			
		knee_r_cs.get("knee_adduction_r").setValue(si, -0.25397256);
		knee_r_cs.get("knee_rotation_r").setValue(si, 0.03301188);
		knee_r_cs.get("knee_anterior_posterior_r").setValue(si, 0.012679);
		knee_r_cs.get("knee_inferior_superior_r").setValue(si, -0.38227168);
		knee_r_cs.get("knee_medial_lateral_r").setValue(si, -0.00403308);
	}
	else if (angle_degrees == -20)
	{
		knee_r_cs.get("knee_angle_r").setValue(si, -0.349066);  // -20 degrees
		
		knee_r_cs.get("knee_adduction_r").setValue(si, -0.295525);
		knee_r_cs.get("knee_rotation_r").setValue(si, 0.0044018);
		knee_r_cs.get("knee_anterior_posterior_r").setValue(si, 0.00522225);
		knee_r_cs.get("knee_inferior_superior_r").setValue(si, -0.382426);
		knee_r_cs.get("knee_medial_lateral_r").setValue(si, -0.00486);
	}
	else if (angle_degrees == -15)
	{
		knee_r_cs.get("knee_angle_r").setValue(si, -0.26179938);  // -15 degrees
 		
		knee_r_cs.get("knee_adduction_r").setValue(si, -0.279252);
		knee_r_cs.get("knee_rotation_r").setValue(si, -0.03060429);
		knee_r_cs.get("knee_anterior_posterior_r").setValue(si, 0.004);
		knee_r_cs.get("knee_inferior_superior_r").setValue(si, -0.384);
		knee_r_cs.get("knee_medial_lateral_r").setValue(si, -0.00391863);
	}
	else if (angle_degrees == 0)
	{
		knee_r_cs.get("knee_angle_r").setValue(si, knee_r_cs.get("knee_angle_r").getDefaultValue());
		//knee_r_cs.get("knee_angle_r").setValue(si, 0);
		
		knee_r_cs.get("knee_adduction_r").setValue(si, knee_r_cs.get("knee_adduction_r").getDefaultValue());
		knee_r_cs.get("knee_rotation_r").setValue(si, knee_r_cs.get("knee_rotation_r").getDefaultValue());
		knee_r_cs.get("knee_anterior_posterior_r").setValue(si, knee_r_cs.get("knee_anterior_posterior_r").getDefaultValue());
		knee_r_cs.get("knee_inferior_superior_r").setValue(si, knee_r_cs.get("knee_inferior_superior_r").getDefaultValue());
		//knee_r_cs.get("knee_inferior_superior_r").setValue(si, -0.385578);
		knee_r_cs.get("knee_medial_lateral_r").setValue(si, knee_r_cs.get("knee_medial_lateral_r").getDefaultValue());
	}

	knee_r_cs.get("knee_angle_r").setLocked(si, true);
	knee_r_cs.get("knee_adduction_r").setValue(si, -0.05235);   // ant load at -90 degrees flexion (+10 degrees)
	//knee_r_cs.get("knee_adduction_r").setValue(si, -0.03490);   // ant load at -80 degrees flexion
	//knee_r_cs.get("knee_adduction_r").setValue(si, -0.122173);   // ant load at -60 degrees flexion
	//knee_r_cs.get("knee_adduction_r").setValue(si, -0.1221730);   // ant load at -40 degrees flexion
	//knee_r_cs.get("knee_adduction_r").setValue(si, -0.191986);   // ant load at -20 degrees flexion
	//knee_r_cs.get("knee_adduction_r").setValue(si, -0.29408);   // ant load at 0 degrees flexion
	knee_r_cs.get("knee_adduction_r").setLocked(si, true);
	//knee_r_cs.get("knee_rotation_r").setLocked(si, true);
	//knee_r_cs.get("knee_anterior_posterior_r").setLocked(si, true);
	//knee_r_cs.get("knee_inferior_superior_r").setLocked(si, true);
	//knee_r_cs.get("knee_medial_lateral_r").setLocked(si, true);
}
