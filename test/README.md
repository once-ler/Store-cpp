* _Commands_ which request that something should happen, ie some state change
* _Events_ which indicate that something has happened
* _Aggregates_ that handles Commands and generates Events based on the current state
  * A Command is always sent to a single Aggregate.
* _Event store_ which stores all events that has happened
* _Application services_ that receives Commands and routes it to the appropriate aggregate
  * Load events for the event store
  * Instantiate a new aggregate
  * Applies all events on the aggregate
  * Sends the command to the aggregate
  * Store the new events

References:
https://blog.jayway.com/2013/03/08/aggregates-event-sourcing-distilled/
https://dev.to/barryosull/event-sourcing-what-it-is-and-why-its-awesome

Event sourcing builds a data model based on event series.

Most enterprise systems built with a concept that of events are generated after data is written to the relational data model.

The reason being they have been using these events primarily for system integration ( messaging ).

The status quo in enterprise is to write to a relational model, then broadcast events based on those changes.

In effect, the events are projections of the relational data, which in my mind is putting the cart before the horse.

This is a flavour of CQRS, and is considered a stepping stone in migrating to an Event Sourced system, rather than the end result.

MSH-9 Message/Event Type
MSH-10 Main Identifier created by sender
MSH-11 Processing ID (D Debugging, T Training, P Production)
MSH-15 Accept Acknowledgement Type (Original: ACID, Enhanced: Eventual Consistency)
MSH-16 Application Acknowledgement Type (AL Always, NE Never, ER Error only, SU Successful only)

# Command is immutable
```
type BaseCommand {
  UUID commandId
}

type SomeCommand<T> : BaseCommand {

}

```

# Create Command Service
```
Response create(Context ctx) {
  sendCommand(SomeCommand<Context>{});
  return Response{};
}

```

# Events are just immutable data
```
type SomeEvent<T> : Event {
  UUID commandId
}
```

# Aggregates
```
// Implement handlers for commands that generate events.  State is not modified.
List<Event> handle(SomeCommand<T> c) {
  return List{
    SomeEvent<A>{},
    SomeEvent<B>{}
  };
}

// Implement handlers for events that modify state
// we only store state in the aggregate that we actually use for command procesing
void handle(SomeEvent<T> e) {
  this.createdAt = now();
  this.data = e.data;
}

```

# EventStore
```
public interface EventStore {
  EventStream loadEventStream(UUID aggregateId);
  void store(UUID aggregateId, long version, List<Event> events);
}

```

#EventStream
```
public interface EventStream : Iterable<event> {
  long version();
}
```

# Application Service
```
public void handle(Command command) throws Exception {
  EventStream<long> eventStream = eventStore.loadEventStream(command.aggregateId());
  Object target = newAggregateInstance(command);
  for (Event event : eventStream) {
    // The event processing is always trivial and should never fail because the event has already happened.
    handle(target, event);
  }
  List<event> events = handle(target, command);
  if (events != null && events.size() > 0) {
    eventStore.store(command.aggregateId(), eventStream.version(), events);
  } else {
    // Command generated no events
  }
}
```
