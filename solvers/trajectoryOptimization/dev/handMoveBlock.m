function handMoveBlock()
block_size = [0.1;0.1;0.1];
block_origin_start = [0;0;block_size(3)/2;0;0;0];
robot = RigidBodyManipulator('block.urdf',struct('floating',true));
robot = robot.addRobotFromURDF('ground.urdf',[0;0;-0.025],[0;0;0]);
robot = robot.addRobotFromURDF('/home/hongkai/Hands/robotiq_hand_description/cfg/s-model_articulated_remove_package.urdf',[],[],struct('floating',true));
nq = robot.getNumPositions();
nv = robot.getNumVelocities();
q0 = zeros(nq,1);
q0(1:3) = [0;0;block_size(3)/2];
q0(9) = 1.5;
object_idx = 2;
finger1_link3 = robot.findLinkInd('finger_1_link_3');
finger1_tip = [0.01;0.01;0];
finger2_link3 = robot.findLinkInd('finger_2_link_3');
finger2_tip = [0.01;0.01;0];
finger3_link3 = robot.findLinkInd('finger_middle_link_3');
finger3_tip = [0.01;0.01;0];
block_side1 = repmat(block_size/2,1,4).*[1 1 -1 -1;1 -1 1 -1;-1 -1 -1 -1];
block_side2 = repmat(block_size/2,1,4).*[1 1 1 1;1 1 -1 -1;1 -1 1 -1];
block_side3 = repmat(block_size/2,1,4).*[-1 -1 -1 -1;1 1 -1 -1;1 -1 1 -1];
block_corners = repmat(block_size/2,1,8).*[1 1 1 1 -1 -1 -1 -1;1 1 -1 -1 1 1 -1 -1;1 -1 1 -1 1 -1 1 -1];

pickup_finger1_cnstr = WorldPositionConstraint(robot,finger1_link3,finger1_tip,block_origin_start(1:3)+block_size/2.*[-1;-0.7;-0.7],block_origin_start(1:3)+block_size/2.*[-1;0.7;0.7]);
pickup_finger2_cnstr = WorldPositionConstraint(robot,finger2_link3,finger2_tip,block_origin_start(1:3)+block_size/2.*[-1;-0.7;-0.7],block_origin_start(1:3)+block_size/2.*[-1;0.7;0.7]);
pickup_finger3_cnstr = WorldPositionConstraint(robot,finger3_link3,finger3_tip,block_origin_start(1:3)+block_size/2.*[1;-0.7;-0.7],block_origin_start(1:3)+block_size/2.*[1;0.7;0.7]);

block_base_idx = (1:6)';
pc_pickup = PostureConstraint(robot);
pc_pickup = pc_pickup.setJointLimits(block_base_idx,block_origin_start,block_origin_start);

hand_base_idx = (7:12)';

Q = ones(nq,1);
Q(1:12) = 0;
Q = diag(Q);
options = IKoptions(robot);
options = options.setQ(Q);
q_pickup = inverseKin(robot,q0,q0,pickup_finger1_cnstr,pickup_finger2_cnstr,pickup_finger3_cnstr,pc_pickup,options);

grasp_idx = 3;
takeoff_idx = 4;
land_idx = 9;
nT = 10;

ground_contact = FrictionConeWrench(robot,object_idx,block_side1,1,[0;0;1]);
finger1_fc = GraspFrictionConeWrench(robot,object_idx, finger1_link3,finger1_tip,[-1;0;0],1);
finger2_fc = GraspFrictionConeWrench(robot,object_idx, finger2_link3,finger2_tip,[-1;0;0],1);
finger3_fc = GraspFrictionConeWrench(robot,object_idx, finger3_link3,finger3_tip,[1;0;0],1);
finger1_contact_wrench = struct('active_knot',grasp_idx:nT,'cw',finger1_fc);
finger2_contact_wrench = struct('active_knot',grasp_idx:nT,'cw',finger2_fc);
finger3_contact_wrench = struct('active_knot',grasp_idx:nT,'cw',finger3_fc);
ground_contact_wrench = struct('active_knot',[],'cw',[]);
ground_contact_wrench(1) = struct('active_knot',1:takeoff_idx,'cw',ground_contact);
ground_contact_wrench(2) = struct('active_knot',land_idx:nT,'cw',ground_contact);


tf_range = [0.4 2];
Q_comddot = eye(3);
Qv = zeros(nv);
Q = zeros(nq);
q_nom = bsxfun(@times,q_pickup,ones(1,nT));
Q_contact_force = 0.01*eye(3);
mcdfkp = ManipulationCentroidalDynamicsFullKinematicsPlanner(robot,object_idx,nT,tf_range,Q_comddot,Qv,Q,q_nom,Q_contact_force,[finger1_contact_wrench finger2_contact_wrench finger3_contact_wrench ground_contact_wrench]);

% bounds on time interval
mcdfkp = mcdfkp.addBoundingBoxConstraint(BoundingBoxConstraint(0.03*ones(nT-1,1),0.1*ones(nT-1,1)),mcdfkp.h_inds(:));


% bounds on initial state
hand_base_start = [0;0;0.5;0;0;0];
mcdfkp = mcdfkp.addBoundingBoxConstraint(ConstantConstraint(reshape(bsxfun(@times,block_origin_start,ones(1,takeoff_idx)),[],1)),reshape(mcdfkp.q_inds(block_base_idx,1:takeoff_idx),[],1));
mcdfkp = mcdfkp.addBoundingBoxConstraint(ConstantConstraint(hand_base_start),mcdfkp.q_inds(hand_base_idx,1));
mcdfkp = mcdfkp.addBoundingBoxConstraint(ConstantConstraint(zeros(nv,1)),mcdfkp.v_inds(:,1));

% bounds on final state
block_origin_final = block_origin_start+[0.3;0;0;0;0;pi/2];
mcdfkp = mcdfkp.addBoundingBoxConstraint(ConstantConstraint(reshape(bsxfun(@times,block_origin_final,ones(1,nT-land_idx+1)),[],1)),reshape(mcdfkp.q_inds(block_base_idx,land_idx:nT),[],1));

% grasp finger tips does not move
finger1_fixed_cnstr = RelativeFixedPositionConstraint(robot,finger1_link3,finger1_tip,object_idx,nT-grasp_idx+1);
finger2_fixed_cnstr = RelativeFixedPositionConstraint(robot,finger2_link3,finger2_tip,object_idx,nT-grasp_idx+1);
finger3_fixed_cnstr = RelativeFixedPositionConstraint(robot,finger3_link3,finger3_tip,object_idx,nT-grasp_idx+1);

mcdfkp = mcdfkp.addNonlinearConstraint(finger1_fixed_cnstr,mcdfkp.q_inds(:,grasp_idx:nT),mcdfkp.kinsol_dataind(grasp_idx:nT));
mcdfkp = mcdfkp.addNonlinearConstraint(finger2_fixed_cnstr,mcdfkp.q_inds(:,grasp_idx:nT),mcdfkp.kinsol_dataind(grasp_idx:nT));
mcdfkp = mcdfkp.addNonlinearConstraint(finger3_fixed_cnstr,mcdfkp.q_inds(:,grasp_idx:nT),mcdfkp.kinsol_dataind(grasp_idx:nT));

% object above the ground
keyboard
end
