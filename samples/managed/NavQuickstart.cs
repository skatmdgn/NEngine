// CS-014: compile-tested public NEngine C# API example.
// Add this script to Assets/Scripts on a project with a NavMeshAgent Scene
// component and a navigation .nnav asset assigned in Project Settings.
using System.Collections;
using NEngine;

public sealed class NavQuickstart : Behaviour
{
    private NavMeshAgent? agent;
    private Coroutine? pending;

    private void Start()
    {
        agent = GetComponent<NavMeshAgent>();
        pending = StartCoroutine(WalkAfterDelay());
    }

    private IEnumerator WalkAfterDelay()
    {
        yield return new WaitForSeconds(0.25f);
        if (agent != null)
        {
            agent.speed = 2.5f;
            agent.stoppingDistance = 0.15f;
            agent.SetDestination(new Vector3(3f, 0f, 2f));
        }
    }

    private void OnDisable()
    {
        if (pending != null)
        {
            StopCoroutine(pending);
            pending = null;
        }
    }
}
