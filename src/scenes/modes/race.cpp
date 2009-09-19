#include "race.h"

WorldRace::WorldRace()
{
}

void WorldRace::init()
{
	testcounter = 1;

	cerr << endl << "Initializing run " << testcounter << " ... " << endl;

	// insert Floor
		makeRaceFloor();

	// autoload critters
	if ( settings->getCVar("autoload") )
		loadAllCritters();

	// insert first batch of critters
		for ( unsigned int i=critters.size(); i < settings->getCVar("mincritters"); i++  )
			insRandomCritter( i );

	// insert food
		for ( unsigned int i=0; i < settings->getCVar("mincritters"); i++  )
			insFood( i );
		
		framecounter = 0;
		haveWinner = false;

	cerr<< "Running" << " ... " << endl;
}

void WorldRace::process()
{
	// do a bullet step
		m_dynamicsWorld->stepSimulation(Timer::Instance()->bullet_ms / 1000000.f);

	// render critter vision, optimized for this sim
		for( unsigned int i=0; i < critters.size(); i++)
			if ( critters[i]->body.mouths.size() > 0 )
			{
				critters[i]->place();
				food[i]->draw();

				for( unsigned int i=0; i < walls.size(); i++)
					walls[i]->draw();
			}

		// Read pixels into retina
		grabVision();

	settings->info_totalNeurons = 0;
	settings->info_totalSynapses = 0;
	settings->info_totalAdamDistance = 0;
	settings->info_totalBodyparts = 0;
	settings->info_totalWeight = 0;

	// process all critters
	for( unsigned int i=0; i < critters.size(); i++)
	{
		CritterB *c = critters[i];

		// TOUCH inputs and references -> find overlappings
			checkCollisions(  c );

		// process
			c->process();

		// process Output Neurons
			if ( c->eat && c->touchingFood )
			{
				Food* f = c->touchedFoodID;
				float eaten = *critter_maxenergy / 100.0f;
				if ( c->energyLevel + eaten > *critter_maxenergy )
					eaten -= (c->energyLevel + eaten) - *critter_maxenergy;
				if ( f->energyLevel - eaten < 0 )
					eaten = f->energyLevel;

				c->energyLevel += eaten;
				f->energyLevel -= eaten;
				
				// if a food unit has no more energy left, we have a winner, the race is over
				if ( f->energyLevel  == 0.0f )
					haveWinner = true;
			}

		// count totals of neurons, synapses and adamdistances
			settings->info_totalNeurons		+= c->brain.totalNeurons;
			settings->info_totalSynapses		+= c->brain.totalSynapses;
			settings->info_totalAdamDistance	+= c->adamdist;
			settings->info_totalBodyparts		+= c->body.bodyparts.size();
			settings->info_totalWeight		+= c->body.totalWeight;
	}

	settings->info_critters = critters.size();
	settings->info_food = food.size();

	framecounter++;
	if ( (haveWinner || framecounter >= settings->getCVar("critter_maxlifetime")) )
	{
		if ( haveWinner )
			cerr << "we have a WINNER after " << framecounter << " frames" << endl;

		cerr << "Evaluating..." << endl;

			// measure their distances from their respective food targets
				for ( unsigned int i=0; i < critters.size(); i++  )
				{
					// fitness function 1: distance to food cube
						btDefaultMotionState* cmyMotionState = (btDefaultMotionState*)critters[i]->body.mouths[0]->body->getMotionState();
						btVector3 cposi = cmyMotionState->m_graphicsWorldTrans.getOrigin();

						btDefaultMotionState* fmyMotionState = (btDefaultMotionState*)food[i]->body.bodyparts[0]->body->getMotionState();
						btVector3 fposi = fmyMotionState->m_graphicsWorldTrans.getOrigin();

						critters[i]->fitness_index =  1.0f /(cposi.distance(fposi) + 0.0000001); 
					
					// fitness function 2: energy of food consumed
						critters[i]->fitness_index += ( (float)settings->getCVar("food_maxenergy") /(food[i]->energyLevel + 0.0000001));

				}

			// initialize sort indices for
				vector<int> indices ( critters.size(), 0 );
				for ( unsigned int i = 0; i < critters.size(); i++ )
					indices[i] = i;
	
			// sort results
				for ( int i = critters.size(); i>0; i--  )
					for ( int j = 0; j < i-1; j++  )
						if ( critters[indices[j]]->fitness_index < critters[indices[j+1]]->fitness_index )
						{
							unsigned keepI	= indices[j];
							indices[j]	= indices[j+1];
							indices[j+1]	= keepI;
						}

			// display results
				for ( unsigned int i=0; i < critters.size(); i++  )
					cerr << "c " << indices[i] << " : " << critters[indices[i]]->fitness_index << endl;

		cerr << endl << "Initializing run " << ++testcounter << " ... " << endl;

			// backup the 50% best critters
				vector<CritterB*> best;

				unsigned int bestNum = critters.size()/2;
				if ( critters.size() == 1 )
					bestNum = 1;
				for ( unsigned int i=0; i < bestNum; i++  )
					best.push_back( new CritterB(*critters[indices[i]], currentCritterID++, btVector3( 0.0f, 0.0f, 0.0f ), false, false) );

			// remove critters and food
				for ( unsigned int i=0; i < critters.size(); i++ )
				{
					if ( critters[i]->isPicked )
						mousepicker->detach();
					delete critters[i];
				}
				critters.clear();
				for ( unsigned int i=0; i < food.size(); i++ )
				{
					if ( food[i]->isPicked )
						mousepicker->detach();
					delete food[i];
				}
				food.clear();

			// clear floor and remake it
				makeRaceFloor();

			// reinsert the best critters
				for ( unsigned int i=0; i < best.size() && i < settings->getCVar("mincritters"); i++  )
					insMutatedCritter( *best[i], critters.size(), false, false );

			// insert the mutants
				unsigned int count = 0;
				while ( critters.size() < settings->getCVar("mincritters") )
				{
					if ( best.size() > 0 )
					{
						bool brainmutant = false;
						bool bodymutant = false;
						if ( randgen->Instance()->get(1,100) <= settings->getCVar("brain_mutationrate") )
							brainmutant = true;

						if ( randgen->Instance()->get(1,100) <= settings->getCVar("body_mutationrate") )
							bodymutant = true;

						insMutatedCritter( *best[count], critters.size(), brainmutant, bodymutant );

						count++;
						if ( count == best.size() && count > 0 )
							count = 0;
					}
					else
						insRandomCritter( critters.size() );
				}

			// remove best again
				for ( unsigned int i=0; i < best.size(); i++  )
					delete best[i];

			// reinsert respective food units
				for ( unsigned int i=0; i < settings->getCVar("mincritters"); i++  )
					insFood( i );

			framecounter = 0;
			haveWinner = false;

			cerr << "Running... " << endl;
	}
}

void WorldRace::makeRaceFloor()
{
	for ( unsigned int i=0; i < walls.size(); i++ )	
		delete walls[i];
	walls.clear();

	critterspacing = (float)settings->getCVar("worldsizeX") / settings->getCVar("mincritters");

	makeFloor();

	// seperator walls
		float WallWidth = 0.2f;
		float WallHalfWidth = WallWidth/2.0f;
		float WallHeight = 1.0f;
		float WallHalfHeight = WallHeight/2.0f;

		for ( unsigned int i=1; i < settings->getCVar("mincritters"); i++  )
		{
			btVector3 position = btVector3 ( 0.0f-WallHalfWidth + (critterspacing*i), WallHalfHeight-WallWidth, settings->getCVar("worldsizeY")/2.0f );
			Wall* w = new Wall( WallWidth, WallHeight, settings->getCVar("worldsizeY"), position, m_dynamicsWorld );
			w->color[0] = 0.34f; w->color[1] = 0.25f; w->color[2] = 0.11f;
			walls.push_back(w);
		}
}

void WorldRace::insRandomCritter(int nr)
{
	CritterB *c = new CritterB(m_dynamicsWorld, currentCritterID++, btVector3( (critterspacing/2)+(critterspacing*nr), 1.0f, settings->getCVar("worldsizeY")-(settings->getCVar("worldsizeY")/4) ), retina);
	c->energyLevel = settings->getCVar("critter_maxenergy") / 2;
	critters.push_back( c );
	c->calcFramePos(critters.size()-1);
}

void WorldRace::insMutatedCritter(CritterB& other, int nr, bool mutateBrain, bool mutateBody)
{
	CritterB *nc;
	nc = new CritterB(other, currentCritterID++, btVector3( (critterspacing/2)+(critterspacing*nr), 1.0f, settings->getCVar("worldsizeY")-(settings->getCVar("worldsizeY")/4) ), mutateBrain, mutateBody);
	nc->energyLevel = settings->getCVar("critter_maxenergy") / 2;
	critters.push_back( nc );
	nc->calcFramePos(critters.size()-1);
}

void WorldRace::insFood(int nr)
{
	Food *f = new Food;
	f->energyLevel = settings->getCVar("food_maxenergy");
	f->createBody( m_dynamicsWorld, btVector3( (critterspacing/2)+(critterspacing*nr), 1.0f, settings->getCVar("worldsizeY")/4 ) );
	food.push_back( f );
}

void WorldRace::insertCritter()
{
	cerr << "inserting critters is disabled during race" << endl;
}

void WorldRace::loadAllCritters()
{
	if ( critters.size() > 0 )
	{
		stringstream buf;
		buf << "use --autoload 1 at commandline to autoload critters into a race";
		Textmessage::Instance()->add(buf);
		cerr << "use --autoload 1 at commandline to autoload critters into a race" << endl;
	}
	else
	{
		vector<string> files;
		dirH.listContentsFull(loaddir, files);

		unsigned int inserted = 0;
		for ( unsigned int i = 0; i < files.size() && inserted < settings->getCVar("mincritters"); i++ )
		{
			if ( parseH->Instance()->endMatches( ".cr", files[i] ) )
			{
				stringstream buf;
				buf << "loading " << files[i];
				Textmessage::Instance()->add(buf);

				string content;
				fileH.open( files[i], content ); 

				critterspacing = (float)settings->getCVar("worldsizeX") / settings->getCVar("mincritters");
				CritterB *c = new CritterB(content, m_dynamicsWorld, btVector3( (critterspacing/2)+(critterspacing*i), 1.0f, settings->getCVar("worldsizeY")-(settings->getCVar("worldsizeY")/4) ), retina);

				unsigned int error = 0;
				if ( c->retinasize != *critter_retinasize ) error = 1;

				if ( !error)
				{
					critters.push_back( c );

					c->critterID = currentCritterID++;
					c->calcFramePos(critters.size()-1);
					c->energyLevel = settings->getCVar("critter_maxenergy") / 2;
					inserted++;
				}
				else
				{
					delete c;
					if ( error == 1 )
					{
						stringstream buf;
						buf << "ERROR: critter retinasize (" << c->retinasize << ") doesn't fit world retinasize (" << *critter_retinasize << ")" << files[i];
						Textmessage::Instance()->add(buf);

						cerr << "ERROR: critter retinasize (" << c->retinasize << ") doesn't fit world retinasize (" << *critter_retinasize << ")" << endl;
					}
				}
			}
		}
		stringstream buf;
		buf << "Loaded critters from " << loaddir;
		Textmessage::Instance()->add(buf);
		//cerr << endl << "Loaded critters from " << loaddir << endl << endl;
	}
}

WorldRace::~WorldRace()
{
}